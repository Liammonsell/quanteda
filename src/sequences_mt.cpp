//#include "dev.h"
#include "quanteda.h"
using namespace quanteda;

int match_bit2(const std::vector<unsigned int> &tokens1, 
              const std::vector<unsigned int> &tokens2){
    
    std::size_t len1 = tokens1.size();
    std::size_t len2 = tokens2.size();
    int bit = 0;
    for (std::size_t i = 0; i < len1 && i < len2; i++) {
        if (tokens1[i] == tokens2[i]) bit += std::pow(2, i); // position dependent
    }
    return bit;
}

// unigram subtuples from B&J algorithm
double sigma_uni(const std::vector<double> &counts, const std::size_t ntokens){
    double s = 0.0;
    s += std::pow(ntokens - 1, 2) / counts[0];
    for (std::size_t b = 0; b < ntokens; b++) {
        s += 1.0 / counts[std::pow(2, b)];
    }
    s += 1.0 / counts[std::pow(2, ntokens) - 1];
    return std::sqrt(s);
}

double lambda_uni(const std::vector<double> &counts, const std::size_t ntokens){
    double l = 0.0;
    l += std::log(counts[0]) * (ntokens - 1); // c0
    for (std::size_t b = 0; b < ntokens; b++) {  //c(b), #(b)=1
        l -= std::log(counts[std::pow(2, b)]);
    }
    l += std::log(counts[std::pow(2, ntokens) - 1]); //c(2^n-1)
    return l;
}

std::size_t bitCount(std::size_t n) {  // count the number of '1' bit
    std::size_t counter = 0;
    while(n) {
        counter += n % 2;
        n >>= 1;
    }
    return counter;
}

// all subtuples from B&J algorithm
double sigma_all(const std::vector<double> &counts){
    const std::size_t n = counts.size();
    double s = 0.0;
    
    for (std::size_t b = 0; b < n; b++) {
        s += 1.0 / counts[b];
    }
    
    return std::sqrt(s);
}

double lambda_all(const std::vector<double> &counts, const std::size_t ntokens){
    const std::size_t n = counts.size();
    
    double l = 0.0;

    for (std::size_t b = 0; b < n; b++) {  //c(b), #(b)=1
        l += std::pow(-1, ntokens - bitCount(b)) * std::log(counts[b]);
    }

    return l;
}
void counts(Text text,
           MapNgrams &counts_seq,
           const unsigned int &len_min,
           const unsigned int &len_max,
           const bool &nested){
    
    if (text.size() == 0) return; // do nothing with empty text
    text.push_back(0); // add padding to include last words
    Ngram tokens_seq;
    tokens_seq.reserve(text.size());
    
    // Collect sequence of specified types
    std::size_t len_text = text.size();
    for (std::size_t i = 0; i < len_text; i++) {
        for (std::size_t j = i; j < len_text; j++) {
            //Rcout << i << " " << j << "\n";
            unsigned int token = text[j];
            bool is_in = true;
            if (token == 0 || j - i >= len_max) {
                is_in = false;
            } 
            
            if (is_in) {
                //Rcout << "Match: " << token << "\n";
                tokens_seq.push_back(token);
            } else {
                //Rcout << "Not match: " <<  token << "\n";
                //only collect sequences that the size of it >= len_min
                if (tokens_seq.size() >= len_min) {  
                    counts_seq[tokens_seq]++;
                }
                tokens_seq.clear();
                if (!nested) i = j; // jump if nested is false
                break;
            }
        }
    }
}

struct counts_mt : public Worker{
    
    Texts texts;
    MapNgrams &counts_seq;
    const unsigned int &len_min;
    const unsigned int &len_max;
    const bool &nested;
    
    
    counts_mt(Texts texts_, MapNgrams &counts_seq_, const unsigned int &len_min_,
             const unsigned int &len_max_, const bool &nested_):
        texts(texts_), counts_seq(counts_seq_), len_min(len_min_), len_max(len_max_), nested(nested_) {}
    
    void operator()(std::size_t begin, std::size_t end){
        for (std::size_t h = begin; h < end; h++){
            counts(texts[h], counts_seq, len_min, len_max, nested);
        }
    }
};

void estimates(std::size_t i,
              VecNgrams &seqs,
              IntParams &cs, 
              DoubleParams &ss, 
              DoubleParams &ls, 
              const String &method,
              const int &count_min){
    
    std::size_t n = seqs[i].size();
    if (n == 1) return; // ignore single words
    if (cs[i] < count_min) return;
    std::vector<double> counts_bit;
    counts_bit.resize(std::pow(2, n), 0.5); // use 1/2 as smoothing
    for (std::size_t j = 0; j < seqs.size(); j++) {
        if (i == j) continue; // do not compare with itself
        //if(ns[j] < count_min) continue; // this is different from the old vesion
        
        int bit;
        bit = match_bit2(seqs[i], seqs[j]);
        counts_bit[bit] += cs[j];
    }
    counts_bit[std::pow(2, n)-1]  += cs[i] - 1;  // c(2^n-1) += number of itself  
    if (method == "unigram"){
        ss[i] = sigma_uni(counts_bit, n);
        ls[i] = lambda_uni(counts_bit, n);
    } else {
        ss[i] = sigma_all(counts_bit);
        ls[i] = lambda_all(counts_bit, n);
    }
}

struct estimates_mt : public Worker{
    
    VecNgrams &seqs;
    IntParams &cs;
    DoubleParams &ss;
    DoubleParams &ls;
    const String &method;
    const unsigned int &count_min;

    // Constructor
    estimates_mt(VecNgrams &seqs_, IntParams &cs_, DoubleParams &ss_, DoubleParams &ls_, const String &method,
                const unsigned int &count_min_):
        seqs(seqs_), cs(cs_), ss(ss_), ls(ls_), method(method), count_min(count_min_){}
    
    void operator()(std::size_t begin, std::size_t end){
        for (std::size_t i = begin; i < end; i++) {
            estimates(i, seqs, cs, ss, ls, method, count_min);
        }
    }
};

/* 
* This funciton estimate the strength of association between specified words 
* that appear in sequences. Estimates are slightly different from the old version,
*  because this faster version does not ignore infrequent sequences.
* @used sequences()
* @creator Kohei Watanabe
* @param texts_ tokens ojbect
* @param count_min sequences appear less than this are ignores
* @param nested if true, subsequences are also collected
* @param method 
*/

// [[Rcpp::export]]
DataFrame qatd_cpp_sequences(const List &texts_,
                             const CharacterVector &types_,
                             const unsigned int count_min,
                             unsigned int len_min,
                             unsigned int len_max,
                             const String &method,
                             bool nested){
    
    Texts texts = as<Texts>(texts_);

    // Collect all sequences of specified words
    MapNgrams counts_seq;
    //dev::Timer timer;
    //dev::start_timer("Count", timer);
#if QUANTEDA_USE_TBB
    counts_mt count_mt(texts, counts_seq, len_min, len_max, nested);
    parallelFor(0, texts.size(), count_mt);
#else
    for (std::size_t h = 0; h < texts.size(); h++) {
        counts(texts[h], counts_seq, len_min, len_max, nested);
    }
#endif
    //dev::stop_timer("Count", timer);
    
    // Separate map keys and values
    std::size_t len = counts_seq.size();
    VecNgrams seqs;
    IntParams cs, ns;
    seqs.reserve(len);
    cs.reserve(len);
    ns.reserve(len);
    for (auto it = counts_seq.begin(); it != counts_seq.end(); ++it) {
        seqs.push_back(it -> first);
        cs.push_back(it -> second);
        ns.push_back(it -> first.size());
    }
    
    // Estimate significance of the sequences
    DoubleParams ss(len);
    DoubleParams ls(len);
    //dev::start_timer("Estimate", timer);
#if QUANTEDA_USE_TBB
    estimates_mt estimate_mt(seqs, cs, ss, ls, method, count_min);
    parallelFor(0, seqs.size(), estimate_mt);
#else
    for (std::size_t i = 0; i < seqs.size(); i++) {
        estimates(i, seqs, cs, ss, ls, method, count_min);
    }
#endif
    //dev::stop_timer("Estimate", timer);
    
    // Convert sequences from integer to character
    CharacterVector seqs_(seqs.size());
    for (std::size_t i = 0; i < seqs.size(); i++) {
        seqs_[i] = join(seqs[i], types_, " ");
    }
    
    DataFrame output_ = DataFrame::create(_["collocation"] = seqs_,
                                          _["count"] = as<IntegerVector>(wrap(cs)),
                                          _["length"] = as<NumericVector>(wrap(ns)),
                                          _["lambda"] = as<NumericVector>(wrap(ls)),
                                          _["sigma"] = as<NumericVector>(wrap(ss)),
                                          _["stringsAsFactors"] = false);
    output_.attr("tokens") = as<Tokens>(wrap(seqs));
    return output_;
}


/***R

toks <- tokens(data_corpus_inaugural)
toks <- tokens_select(toks, stopwords("english"), "remove", padding = TRUE)

toks <- tokens_select(toks, "^([A-Z][a-z\\-]{2,})", valuetype="regex", case_insensitive = FALSE, padding = TRUE)
types <- unique(as.character(toks))
out2 <- qatd_cpp_sequences(toks, types, 1, 2, 2, "unigram",TRUE)
out3 <- qatd_cpp_sequences(toks, types, 1, 2, 2, "all_subtuples",TRUE)
out4 <- qatd_cpp_sequences(toks, types, 1, 2, 3, "unigram",TRUE)
# out2$z <- out2$lambda / out2$sigma
# out2$p <- 1 - stats::pnorm(out2$z)


*/
