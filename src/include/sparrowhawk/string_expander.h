
#include <string>
#include <fst-decl.h>





using std::string;
using fst::Fst;

namespace speech {
namespace alignment {

class StringExpander {
  public:
    explicit StringExpander(Fst * _verbalizer);
    virtual ~StringExpander();

    void expand(string transcript, Fst* output, string delim=" ");

    static DEBUG = true;
    static string IMAGE_DIR = "/home/bupenieks/tts/image/";

  private:
    Fst * verbalizer;

    void format_and_save_fst(Fst* fst, string name);
}

} // namespace alignment
} // namespace speech
