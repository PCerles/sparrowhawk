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
// Very simple stand-alone binary to run Sparrowhawk normalizer on a line of
// text.
//
// It runs the sentence boundary detector on the input, and then normalizes each
// sentence.
//
// As an example of use, build the test data here, and put them somewhere, such
// as tmp/sparrowhawk_test
//
// Then copy the relevant fars and protos there, edit the protos and then run:
//
// blaze-bin/speech/tts/open_source/sparrowhawk/normalizer_main \
//  --config tmp/sparrowhawk_test/sparrowhawk_configuration_af.ascii_proto
//
// Then input a few sentences on one line such as:
//
// Kameelperde het 'n kenmerkende voorkoms, met hul lang nekke en relatief \
// kort lywe. Hulle word 4,3 - 5,7m lank. Die bulle is effens langer as die \
// koeie.

#include <iostream>
#include <memory>
#include <string>
using std::string;
#include <vector>
#include <assert.h>
using std::vector;

#include <sparrowhawk/normalizer.h>

DEFINE_bool(multi_line_text, false, "Text is spread across multiple lines.");
DEFINE_string(config, "", "Path to the configuration proto.");
DEFINE_string(path_prefix, "./", "Optional path prefix if not relative.");

void NormalizeInput(const string& input,
                    speech::sparrowhawk::Normalizer *normalizer) {
  const std::vector<string> sentences = normalizer->SentenceSplitter(input);
  for (const auto& sentence : sentences) {
    string output;
    normalizer->Normalize(sentence, &output);
    std::cout << sentence << "|" << output << std::endl;
  }
}

void NormalizeInputSingleWord(const string& input,
                    speech::sparrowhawk::Normalizer *normalizer) {
    string output;
    normalizer->Normalize(input, &output);
    std::cout << input << "|" << output << std::endl;
}


void BuildSymbolTable(fst::SymbolTable * output) {
    assert (output->AddSymbol("<eps>") == 0);
    assert (output->AddSymbol("I")  ==  1);
    assert (output->AddSymbol("ATE") == 2);
    assert (output->AddSymbol("TEN") == 3);
    assert (output->AddSymbol("10") == 4);
    assert (output->AddSymbol("ONE") == 5);
    assert (output->AddSymbol("ZERO") == 6);
    assert (output->AddSymbol("DONUTS") == 7);
    assert (output->AddSymbol(" ") == 8);
}

int main(int argc, char** argv) {
  using speech::sparrowhawk::Normalizer;
  std::set_new_handler(FailedNewHandler);
  SET_FLAGS(argv[0], &argc, &argv, true);
  std::unique_ptr<Normalizer> normalizer;
  normalizer.reset(new Normalizer());
  CHECK(normalizer->Setup(FLAGS_config, FLAGS_path_prefix));
  string input;

  fst::SymbolTable syms("test.syms");
  BuildSymbolTable(&syms);

  fst::StdVectorFst fst;
  string in;// = "$10.01 abc 123 Jan. 1 2007";// test www.google.com";

  int num = 0;
  while (std::getline(std::cin, in))      {
      normalizer->ConstructVerbalizer(in, &fst, &syms);
      normalizer->format_and_save_fst(&fst, std::to_string(num++).c_str());
  }


  return 0;

  if (FLAGS_multi_line_text) {
    string line;
    while (std::getline(std::cin, line)) {
      if (!input.empty()) input += " ";
      input += line;
    }
    NormalizeInputSingleWord(input, normalizer.get());
  } else {
    while (std::getline(std::cin, input)) {
      NormalizeInputSingleWord(input, normalizer.get());
    }
  }
  return 0;
}
