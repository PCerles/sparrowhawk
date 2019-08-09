// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2015 and onwards Google, Inc.
#include <sparrowhawk/normalizer.h>

#include <memory>
#include <string>
#include <assert.h>
#include <fst/script/print.h>
using std::string;

#include <fst/arcsort.h>
#include <fst/closure.h>
#include <google/protobuf/text_format.h>
#include <sparrowhawk/items.pb.h>
#include <sparrowhawk/sentence_boundary.h>
#include <sparrowhawk/serialization_spec.pb.h>
#include <sparrowhawk/sparrowhawk_configuration.pb.h>
#include <sparrowhawk/io_utils.h>
#include <sparrowhawk/logger.h>
#include <sparrowhawk/protobuf_parser.h>
#include <sparrowhawk/protobuf_serializer.h>
#include <sparrowhawk/spec_serializer.h>
#include <sparrowhawk/string_utils.h>

namespace speech {
namespace sparrowhawk {

typedef fst::StringPrinter<fst::StdArc> Printer;
// TODO(rws): We actually need to do something with this.
const char kDefaultSentenceBoundaryRegexp[] = "[\\.:!\\?] ";

Normalizer::Normalizer() { }

Normalizer::~Normalizer() { }


bool Normalizer::Setup(const string &configuration_proto,
                       const string &pathname_prefix) {

  SparrowhawkConfiguration configuration;
  string proto_string = IOStream::LoadFileToString(pathname_prefix +
                                                   "/" + configuration_proto);
  if (!google::protobuf::TextFormat::ParseFromString(proto_string, &configuration))
    return false;
  if (!(configuration.has_tokenizer_grammar()))
    LoggerError("Configuration does not define a tokenizer-classifier grammar");
  if (!(configuration.has_verbalizer_grammar()))
    LoggerError("Configuration does not define a verbalizer grammar");
  tokenizer_classifier_rules_.reset(new RuleSystem);
  if (!tokenizer_classifier_rules_->LoadGrammar(
          configuration.tokenizer_grammar(),
          pathname_prefix))
    return false;
  verbalizer_rules_.reset(new RuleSystem);
  if (!verbalizer_rules_->LoadGrammar(configuration.verbalizer_grammar(),
                                      pathname_prefix))
    return false;
  string sentence_boundary_regexp;
  if (configuration.has_sentence_boundary_regexp()) {
    sentence_boundary_regexp = configuration.sentence_boundary_regexp();
  } else {
    sentence_boundary_regexp = kDefaultSentenceBoundaryRegexp;
  }
  sentence_boundary_.reset(new SentenceBoundary(sentence_boundary_regexp));
  if (configuration.has_sentence_boundary_exceptions_file()) {
    if (!sentence_boundary_->LoadSentenceBoundaryExceptions(
            pathname_prefix + '/' + configuration.sentence_boundary_exceptions_file())) {
      LoggerError("Cannot load sentence boundary exceptions file: %s",
                  configuration.sentence_boundary_exceptions_file().c_str());
    }
  }
  if (configuration.has_serialization_spec()) {
    string spec_string = IOStream::LoadFileToString(
        pathname_prefix + "/" + configuration.serialization_spec());
    SerializeSpec spec;
    if (spec_string.empty() ||
        !google::protobuf::TextFormat::ParseFromString(spec_string, &spec) ||
        (spec_serializer_ = Serializer::Create(spec)) == nullptr) {
      LoggerError("Failed to load a valid serialization spec from file: %s",
                  configuration.serialization_spec().c_str());
      return false;
    }
  }
//  LoggerDebug("setting up normalizer");
  return true;
}

bool Normalizer::Normalize(const string &input, string *output) const {
  std::unique_ptr<Utterance> utt;
  utt.reset(new Utterance);
  if (!Normalize(utt.get(), input)) return false;
  *output = LinearizeWords(utt.get());
  return true;
}

bool Normalizer::Normalize(Utterance *utt, const string &input) const {
  return TokenizeAndClassifyUtt(utt, input) && VerbalizeUtt(utt);
}

bool Normalizer::NormalizeAndShowLinks(
    const string &input, string *output) const {
  std::unique_ptr<Utterance> utt;
  utt.reset(new Utterance);
  if (!Normalize(utt.get(), input)) return false;
  *output = ShowLinks(utt.get());
  return true;
}




bool Normalizer::TokenizeAndClassifyUtt(Utterance *utt,
                                        const string &input) const {
  typedef fst::StringCompiler<fst::StdArc> Compiler;
  Compiler compiler(fst::StringTokenType::BYTE);
  MutableTransducer input_fst, output;
  if (!compiler(input, &input_fst)) {
    LoggerError("Failed to compile input string \"%s\"", input.c_str());
    return false;
  }
  if (!tokenizer_classifier_rules_->ApplyRules(input_fst,
                                               &output,
                                               true /*  use_lookahead */)) {
    LoggerError("Failed to tokenize \"%s\"", input.c_str());
    return false;
  }
  MutableTransducer shortest_path;
  fst::ShortestPath(output, &shortest_path);
  ProtobufParser parser(&shortest_path);
  if (!parser.ParseTokensFromFST(utt, true /* set SEMIOTIC_CLASS */)) {
    LoggerError("Failed to parse tokens from FST for \"%s\"", input.c_str());
    return false;
  }
  return true;
}

// As in Kestrel's Run(), this processes each token in turn and creates the Word
// stream, adding words each with a unique wordid.  Takes a different action on
// the type:
//
// PUNCT: do nothing
// SEMIOTIC_CLASS: call verbalizer FSTs
// WORD: add to word stream
bool Normalizer::VerbalizeUtt(Utterance *utt) const {
  for (int i = 0; i < utt->linguistic().tokens_size(); ++i) {
    Token *token = utt->mutable_linguistic()->mutable_tokens(i);
    string token_form = ToString(*token);
    token->set_first_daughter(-1);  // Sets to default unset.
    token->set_last_daughter(-1);   // Sets to default unset.
    // Add a single silence for punctuation that forms phrase breaks. This is
    // set via the grammar, though ultimately we'd like a proper phrasing
    // module.
    if (token->type() == Token::PUNCT) {
      if (token->phrase_break() &&
          (utt->linguistic().words_size() == 0 ||
           utt->linguistic().words(
               utt->linguistic().words_size() - 1).id() != "sil")) {
        AddWord(utt, token, "sil");
      }
    } else if (token->type() == Token::SEMIOTIC_CLASS) {
      if (!token->skip()) {
        LoggerDebug("Verbalizing: [%s]\n", token_form.c_str());
        string words;
        if (VerbalizeSemioticClass(*token, &words)) {
          AddWords(utt, token, words);
        } else {
          LoggerWarn("First-pass verbalization FAILED for [%s]",
                     token_form.c_str());
          // Back off to verbatim reading
          string original_token = token->name();
          token->Clear();
          token->set_name(original_token);
          token->set_verbatim(original_token);
          if (VerbalizeSemioticClass(*token, &words)) {
            LoggerWarn("Reversion to verbatim succeeded for [%s]",
                       original_token.c_str());
            AddWords(utt, token, words);
          } else {
            // If we've done our checks right, we should never get here
            LoggerError("Verbalization FAILED for [%s]", token_form.c_str());
          }
        }
      }
    } else if (token->type() == Token::WORD) {
      if (token->has_wordid()) {
        AddWord(utt, token, token->wordid());
      } else {
        LoggerError("Token [%s] has type WORD but there is no word id",
                    token_form.c_str());
      }
    } else {
      LoggerError("No type found for [%s]", token_form.c_str());
    }
  }
  LoggerDebug("Verbalize output: Words\n%s\n\n", LinearizeWords(utt).c_str());
  return true;
}

bool Normalizer::VerbalizeSemioticClass(const Token &markup,
                                        string *words) const {
  Token local(markup);
  CleanFields(&local);
  MutableTransducer input_fst;
  if (spec_serializer_ == nullptr) {
    ProtobufSerializer serializer(&local, &input_fst);
    serializer.SerializeToFst();
  } else {
    input_fst = spec_serializer_->Serialize(local);
  }
  if (!verbalizer_rules_->ApplyRules(input_fst,
                                     words,
                                     false /* use_lookahead */)) {
    LoggerError("Failed to verbalize \"%s\"", ToString(local).c_str());
    return false;
}
}



bool Normalizer::VerbalizeSemioticClass(const Token &markup, std::vector<MutableTransducer>* output) const {
   Token local(markup);
   CleanFields(&local);
   MutableTransducer input_fst;
   if (spec_serializer_ == nullptr) {
     ProtobufSerializer serializer(&local, &input_fst);
     serializer.SerializeToFst();
   } else {
     input_fst = spec_serializer_->Serialize(local);
   }

   if (!verbalizer_rules_->ApplyRules(input_fst,
                                      output,
                                      false /* use_lookahead */)) {
     LoggerError("Failed to verbalize \"%s\"", ToString(local).c_str());
     return false;
   }

   return true;

}

std::vector<string> Normalizer::SentenceSplitter(const string &input) const {
  return sentence_boundary_->ExtractSentences(input);
}


std::vector<MutableTransducer> Normalizer::TokenizeAndVerbalize(string word, MutableTransducer* res_output) {
    typedef fst::StringCompiler<fst::StdArc> Compiler;
    Compiler compiler(fst::StringTokenType::BYTE);
    fst::VectorFst<fst::StdArc> transcript_lm;
    if (!compiler(word, &transcript_lm)) {
        printf("Failed to compile input string \"%s\"", word.c_str());
        return {};
    }

    std::vector<MutableTransducer> tok_fsts;
    if (!tokenizer_classifier_rules_->ApplyRules(transcript_lm,
                                                  &tok_fsts,
                                                  true /*  use_lookahead */)) {
       LoggerError("Failed to tokenize");
       return {};
    }

    std::vector<MutableTransducer> verbalized;
    std::vector<string> out;
    for (MutableTransducer shortest_path : tok_fsts) {

      Utterance utt;
      ProtobufParser parser(&shortest_path);

      if (!parser.ParseTokensFromFST(&utt, true /* set SEMIOTIC_CLASS */)) {
        LoggerError("Failed to parse tokens from FST");
        continue;
      }

      Compiler compiler(fst::StringTokenType::BYTE);
      MutableTransducer concatenated_output;
      concatenated_output.AddState();
      concatenated_output.SetStart(0);
      concatenated_output.SetFinal(0, 0.0);


      for (int i = 0; i < utt.linguistic().tokens_size(); ++i) {
          std::vector<MutableTransducer> output_verbalizations; 
          Token *token = utt.mutable_linguistic()->mutable_tokens(i);
          string token_form = ToString(*token);
            
          token->set_first_daughter(-1);  // Sets to default unset.
          token->set_last_daughter(-1);   // Sets to default unset.
          // Add a single silence for punctuation that forms phrase breaks. This is
          // set via the grammar, though ultimately we'd like a proper phrasing
          // module.
          if (token->type() == Token::PUNCT) {
            if (token->phrase_break() &&
                (utt.linguistic().words_size() == 0 ||
                 utt.linguistic().words(
                     utt.linguistic().words_size() - 1).id() != "sil")) {
                
                // insert epsilon for punctuation 
                MutableTransducer output;
                if (!CompileStringToEpsilon("", &output)) {
                    break;
                }
                output_verbalizations.push_back(output);
            }
          } else if (token->type() == Token::SEMIOTIC_CLASS) {
            if (!token->skip()) {
              //LoggerDebug("Verbalizing: [%s]\n", token_form.c_str());
              string words;
              if (VerbalizeSemioticClass(*token, &output_verbalizations)) {
              } else {
                break;
                LoggerWarn("First-pass verbalization FAILED for [%s]",
                           token_form.c_str());
                // Back off to verbatim reading
                string original_token = token->name();
                token->Clear();
                token->set_name(original_token);
                token->set_verbatim(original_token);
                if (VerbalizeSemioticClass(*token, &output_verbalizations)) {
                  LoggerWarn("Reversion to verbatim succeeded for [%s]",
                             original_token.c_str());
                } else {
                  // If we've done our checks right, we should never get here
                  LoggerError("Verbalization FAILED for [%s]", token_form.c_str());
              //continue;
                break;
                }
              }
            }
          } else if (token->type() == Token::WORD) {
            if (token->has_wordid()) {
                // Writes original string 
                MutableTransducer output;
                if (!compiler(token->wordid(), &output)) {
                    printf("Failed to compile input string \"%s\"", token->wordid().c_str());
                    //continue;
                    break;
                }
                output_verbalizations.push_back(output);
            } else {
              LoggerError("Token [%s] has type WORD but there is no word id",
                          token_form.c_str());
              //continue;
              break;
            }
          } else {
            LoggerError("No type found for [%s]", token_form.c_str());
            //continue;
            break;
          }
 
         // Project and Optimize 
         MutableTransducer verbalization_union;
         verbalization_union.AddState();
         verbalization_union.SetStart(0);
         verbalization_union.SetFinal(0, 0.0);

         for (MutableTransducer verbalization : output_verbalizations) {
             fst::Project(&verbalization, fst::PROJECT_OUTPUT);
            
             MutableTransducer temp;
             fst::ShortestPath(verbalization, &temp);
             verbalization = std::move(temp);

             fst::Determinize<fst::StdArc>(verbalization, &temp);
             verbalization = std::move(temp);
             fst::Minimize(&verbalization);

             fst::RmEpsilon(&verbalization);

             fst::Union(&verbalization_union, verbalization);
        }

        fst::RmEpsilon(&verbalization_union);
        verbalization_union.SetFinal(0, fst::StdArc::Weight::Zero());

        AddDifferentVerbalization(&verbalization_union, "zero", "oh");

        // Create space fst for concatenation of words
        MutableTransducer space;
        space.AddState();
        space.AddState();
        space.AddArc(0, fst::StdArc(0, ' ', 0.0, 1));
        space.SetStart(0);
        space.SetFinal(1, 0.0);
      
        if (i > 0)
           fst::Concat(&concatenated_output, space);

        fst::Concat(&concatenated_output, verbalization_union);
      }

   

      verbalized.push_back(concatenated_output);
      //LoggerDebug("Verbalize output: Words\n%s\n\n", LinearizeWords(&utt).c_str());
    }

        return verbalized;
}


bool Normalizer::CompileStringToEpsilon(string s, MutableTransducer* output) {
    MutableTransducer word;
    int index = 0;
    for (char c : s) {
        word.AddState();
        word.AddArc(index, fst::StdArc(c, 0, 0.0, index+1));
        ++index;
    }
    word.AddState();

    word.SetStart(0);

    word.SetFinal(s.length(), 0.0);

    *output = std::move(word);
}

std::string Normalizer::ConstructVerbalizerString(string transcript) {
    MutableTransducer acceptor;
    ConstructVerbalizer(transcript, &acceptor);

    std::ostringstream outs;
    fst::script::PrintFst(acceptor, outs);
    string output_string = outs.str();
    return output_string;
}

void Normalizer::ConstructVerbalizer(string transcript, MutableTransducer* output) {
    
    typedef fst::StringCompiler<fst::StdArc> Compiler;
    Compiler compiler(fst::StringTokenType::BYTE);

    // Separate words by spaces
    std::istringstream iss(transcript);
    vector<string> words { std::istream_iterator<string>{iss},
                           std::istream_iterator<string>{}     };

    // Create space fst for concatenation of words 
    MutableTransducer space;
    space.AddState();
    space.AddState();
    space.AddArc(0, fst::StdArc(0, ' ', 0.0, 1));
    space.SetStart(0);
    space.SetFinal(1, 0.0);

    // Init verbalizer
    MutableTransducer verbalizer;
    verbalizer.AddState();
    verbalizer.SetStart(0);
    verbalizer.SetFinal(0, 0.0);
    // Construct verbalizer 
    bool has_word = false;
    for (string word : words) {
        MutableTransducer output, word_fst;

        std::vector<MutableTransducer> verbalized_strings = TokenizeAndVerbalize(word, &output);

        word_fst = verbalized_strings[0];
        for (MutableTransducer expansion : verbalized_strings) {
            fst::Union(&word_fst, expansion);
        }
    
        fst::Project(&word_fst, fst::PROJECT_OUTPUT);
        fst::RmEpsilon(&word_fst);
      
        // Merge final states
        fst::StdArc::StateId new_state_id = word_fst.AddState();
        for (fst::StateIterator<fst::StdFst> siter(word_fst); !siter.Done(); siter.Next())  {
            fst::StdArc::StateId state_id = siter.Value();
            fst::StdArc::Weight weight = word_fst.Final(state_id);
            if (weight != fst::StdArc::Weight::Zero()) {
               word_fst.SetFinal(state_id, fst::StdArc::Weight::Zero());
               word_fst.AddArc(state_id, fst::StdArc(0,0,0.0,new_state_id));
            }
        }
        word_fst.SetFinal(new_state_id, fst::StdArc::Weight::One());
        
        


        MutableTransducer temp;
        fst::Determinize<fst::StdArc>(word_fst, &temp);
        word_fst = std::move(temp);

        fst::Minimize(&word_fst);

        if (has_word) fst::Concat(&verbalizer, space);
        fst::Concat(&verbalizer, word_fst);
        has_word = true;
    }

    MutableTransducer det;
    fst::Project(&verbalizer, fst::PROJECT_OUTPUT);
    fst::RmEpsilon(&verbalizer);

    // Merge final state       
    fst::StdArc::StateId new_state_id = verbalizer.AddState();
    for (fst::StateIterator<fst::StdFst> siter(verbalizer); !siter.Done(); siter.Next())  {
        fst::StdArc::StateId state_id = siter.Value();
        fst::StdArc::Weight weight = verbalizer.Final(state_id);
        if (weight != fst::StdArc::Weight::Zero()) {
           verbalizer.SetFinal(state_id, fst::StdArc::Weight::Zero());
           verbalizer.AddArc(state_id, fst::StdArc(0,0,0.0,new_state_id));
        }
    }
    verbalizer.SetFinal(new_state_id, fst::StdArc::Weight::One());

    format_and_save_fst(&verbalizer, "verbalizer");
    *output = std::move(verbalizer);
}


void Normalizer::AddDifferentVerbalization(MutableTransducer* fst, char const* orig_verb, char * new_verb) const {

    for (fst::StateIterator<fst::StdFst> siter(*fst); !siter.Done(); siter.Next())  {
        std::vector<fst::StdArc::StateId> path_endpoints = FindPath( orig_verb, fst, siter.Value() );
        
        for (fst::StdArc::StateId end_point : path_endpoints) {
            fst::StdArc::StateId prev_state = siter.Value();

            // Iterate over each character and add a new arc
            for (char * c_iter = new_verb; *c_iter != '\0'; c_iter++) {
                fst::StdArc::StateId new_state = fst->AddState();
                fst->AddArc(prev_state, fst::StdArc(*c_iter, *c_iter, 0.0, new_state));
                prev_state = new_state;
            }
            
            fst->AddArc(prev_state, fst::StdArc(0,0,0.0,end_point));
        }
    }
}

std::vector<fst::StdArc::StateId> Normalizer::FindPath(char const * s, MutableTransducer * fst, fst::StdArc::StateId state) const {
    using PathEndpoint = fst::StdArc::StateId;
    std::vector<PathEndpoint> paths;
    char current_char = *s;

    if (current_char == '\0') return { state };

    // Find arcs that have 'current_char' as their label
    fst::Matcher<fst::StdFst> matcher(fst, fst::MATCH_INPUT);
    matcher.SetState(state);
    if (matcher.Find(current_char)) {
        for(; !matcher.Done(); matcher.Next()) {
            const fst::StdArc &arc = matcher.Value();

            // Remove first character of string and recursively find endpoint of path
            if (arc.olabel == current_char) {
                std::vector<PathEndpoint> tail = FindPath(s+1, fst, arc.nextstate);
                paths.insert(paths.end(), tail.begin(), tail.end()); // Concatenate vectors
            }
        }
    }

    return paths;
}

//    // Rewrite dashes
//    MutableTransducer dash_rewrite;
//    dash_rewrite.AddState();
//    dash_rewrite.AddState();
//    dash_rewrite.AddArc(0, fst::StdArc('-', 0, 0.0, 1));
//    dash_rewrite.SetStart(0);
//    dash_rewrite.SetFinal(1, 0.0);
//
//    MutableTransducer dash_rewrite_suffix;
//    std::vector<string> expansions { " to ", " minus ", " through " };
//    for (string exp : expansions) {
//        MutableTransducer string_fst;
//        CompileStringToEpsilon(exp, &string_fst);       
//        fst::Invert(&string_fst);
//        fst::Union(&dash_rewrite_suffix, string_fst);
//        break;
//    }
//
//    fst::Concat(&dash_rewrite, dash_rewrite_suffix);
//    
//    // Merge final state       
//    new_state_id = dash_rewrite.AddState();
//    for (fst::StateIterator<fst::StdFst> siter(dash_rewrite); !siter.Done(); siter.Next())  {
//        fst::StdArc::StateId state_id = siter.Value();
//        fst::StdArc::Weight weight = dash_rewrite.Final(state_id);
//        if (weight != fst::StdArc::Weight::Zero()) {
//           dash_rewrite.SetFinal(state_id, fst::StdArc::Weight::Zero());
//           dash_rewrite.AddArc(state_id, fst::StdArc(0,0,0.0,new_state_id));
//        }
//    }
//    dash_rewrite.SetFinal(new_state_id, fst::StdArc::Weight::One());
//    fst::Minimize(&dash_rewrite);
//    fst::RmEpsilon(&dash_rewrite);
//
//    ArcSort(&dash_rewrite, fst::ILabelCompare<fst::StdArc>());
//
//    Closure(&dash_rewrite, fst::CLOSURE_PLUS);
//
//    format_and_save_fst(&dash_rewrite, "dash_rewrite", "/tts/images2/");
//    
//    MutableTransducer t;
//
//    t.AddState();
//    t.AddState();
//    t.AddState();
//    t.AddArc(0, fst::StdArc('a', 'a', 0.0, 1));
//    t.AddArc(1, fst::StdArc('-', '-', 0.0, 2));
//    t.SetStart(0);
//    t.SetFinal(2, 0.0);
//
//
//    std::vector<MutableTransducer> app;
//    if (!verbalizer_rules_->ApplyRules(verbalizer,
//                                     &app,
//                                     false /* use_lookahead */)) {
//    LoggerError("Failed to verbalize dashes");
//    return ;
//}
    //fst::ShortestPath(verbalizer, &t);
//    int abc = 0;
//    for (auto ko : app) {
//    format_and_save_fst(&ko, "t234567" + abc++, "/tts/images2/");
//    }
//    fst::Compose<fst::StdArc>(t, dash_rewrite, output);

///}

void Normalizer::format_and_save_fst(MutableTransducer * fst, char const * name, char const * IMAGE_DIR) const {
    char buf[100];
    sprintf(buf, "%s/%s.fst", IMAGE_DIR, name);
    fst->Write(buf);
    std::cout << name << " written" << std::endl;
}

}  // namespace sparrowhawk
}  // namespace speech
