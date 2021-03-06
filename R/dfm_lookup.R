#' apply a dictionary to a dfm
#' 
#' Apply a dictionary to a dfm by looking up all dfm features for matches in a
#' a set of \link{dictionary} values, and combine replace those features with a 
#' count of the dictionary's keys.  If \code{exclusive = FALSE} then the 
#' behaviour is to apply a "thesaurus" where each value match is replaced by 
#' the dictionary key, converted to capitals if \code{capkeys = TRUE} (so that 
#' the replacements are easily distinguished from features that were terms
#' found originally in the document).
#' @param x the dfm to which the dictionary will be applied
#' @param dictionary a \link{dictionary} class object
#' @param levels levels of entries in a hierachical dictionary that will be 
#'   applied
#' @param exclusive if \code{TRUE}, remove all features not in dictionary, 
#'   otherwise, replace values in dictionary with keys while leaving other 
#'   features unaffected
#' @inheritParams valuetype
#' @param case_insensitive ignore the case of dictionary values if \code{TRUE}
#' @param capkeys if \code{TRUE}, convert dictionary keys to
#'   uppercase to distinguish them from other features
#' @param verbose print status messages if \code{TRUE}
#' @export
#' @note \code{dfm_lookup} should not be used with dictionaries containing
#' multi-word values, because dfm features will already have been fixed using
#' a specific ngram value which may not match the multi-word structure of the
#' dictionary.
#' @keywords dfm
#' @examples
#' myDict <- dictionary(list(christmas = c("Christmas", "Santa", "holiday"),
#'                           opposition = c("Opposition", "reject", "notincorpus"),
#'                           taxglob = "tax*",
#'                           taxregex = "tax.+$",
#'                           country = c("United_States", "Sweden")))
#' myDfm <- dfm(c("My Christmas was ruined by your opposition tax plan.", 
#'                "Does the United_States or Sweden have more progressive taxation?"),
#'              remove = stopwords("english"), verbose = FALSE)
#' myDfm
#' 
#' # glob format
#' dfm_lookup(myDfm, myDict, valuetype = "glob")
#' dfm_lookup(myDfm, myDict, valuetype = "glob", case_insensitive = FALSE)
#'
#' # regex v. glob format: note that "united_states" is a regex match for "tax*"
#' dfm_lookup(myDfm, myDict, valuetype = "glob")
#' dfm_lookup(myDfm, myDict, valuetype = "regex", case_insensitive = TRUE)
#' 
#' # fixed format: no pattern matching
#' dfm_lookup(myDfm, myDict, valuetype = "fixed")
#' dfm_lookup(myDfm, myDict, valuetype = "fixed", case_insensitive = FALSE)
dfm_lookup <- function(x, dictionary, levels = 1:5,
                       exclusive = TRUE, valuetype = c("glob", "regex", "fixed"), 
                       case_insensitive = TRUE,
                       capkeys = !exclusive,
                       verbose = quanteda_options("verbose")) {
    
    if (!is.dfm(x))
        stop("x must be a dfm object")
    
    if (!is.dictionary(dictionary))
        stop("dictionary must be a dictionary object")
    
    dictionary <- flatten_dictionary(dictionary, levels)
    valuetype <- match.arg(valuetype)
    attrs <- attributes(x)
    
    if (has_multiword(dictionary) && x@ngrams == 1) {
        stop("dfm_lookup not implemented for ngrams > 1 and multi-word dictionary values")
    }
    
    # Generate all combinations of type IDs
    entries_id <- list()
    keys_id <- c()
    types <- featnames(x)
    
    if (verbose) 
        catm("applying a dictionary consisting of ", length(dictionary), " key", 
             if (length(dictionary) > 1L) "s" else "", "\n", sep="")
    
    for (h in seq_along(dictionary)) {
        entries <- dictionary[[h]]
        entries_temp <- regex2id(as.list(entries), types, valuetype, case_insensitive, FALSE)
        entries_id <- c(entries_id, entries_temp)
        keys_id <- c(keys_id, rep(h, length(entries_temp)))
    }
    if (length(entries_id)) {
        
        if (capkeys) {
            keys <- char_toupper(names(dictionary))
        } else {
            keys <- names(dictionary)
        }
        temp <- x[,unlist(entries_id, use.names = FALSE)]
        colnames(temp) <- keys[keys_id]
        temp <- dfm_compress(temp, margin = 'features')
        temp <- dfm_select(temp, features = as.list(keys), valuetype = 'fixed', padding = TRUE)
        
        if (exclusive) {
            result <- temp[,keys]
        } else {
            result <- cbind(x[,unlist(entries_id) * -1], temp[,keys])
        }
    } else {
        if (exclusive) {
            result <- x[,0] # dfm without features
        } else {
            result <- x
        }
    }
    attr(result, "what") <- "dictionary"
    attr(result, "dictionary") <- dictionary
    attributes(result, FALSE) <- attributes(x)
    return(result)
}


