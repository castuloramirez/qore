#!/usr/bin/env qore

%enable-all-warnings
%disable-warning undeclared-var

# Examples of qore regular expressions
# initially written and tested for qore 0.4.0 (Oct 2005)
# by Helmut Wollmersdorfer

# TODO: check against http://www.opengroup.org/onlinepubs/007908799/xbd/re.html

# regex in qore should follow POSIX 1003.2 'extended'
# actual behaviour depends on the used (g)libc

# The following examples are in the order of regex(7) of Debian/Sarge
# Comments beginning with '##' are quotes from regex(7)

## 1003.2 leaves some aspects of RE syntax and semantics open; 
## '(!)' marks decisions  on these  aspects that may not be fully 
## portable to other 1003.2 implementations.

## A (modern) RE is one(!) or more non-empty(!) branches, separated  by '|'.
## It matches anything that matches one of the branches.
$t = 'Branches';          # text
$s = 'abc';               # string
$p = 'a|z';               # pattern
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a|z/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'qrs';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a|z/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## A  branch  is  one(!) or more pieces, concatenated.  It matches a match
## for the first, followed by a match for the second, etc.
$t = 'Pieces';
$s = 'abcxyz';
$p = 'bc';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /bc/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'bac';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /bc/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## A piece is an atom possibly followed by a single(!) '*', '+',  '?',  or bound.
$t = 'Atoms and repeaters';
$s = 'abcxyz';
$p = 'ab*c+x?y{1}';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /ab*c+x?y{1}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'bac';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /ab*c+x?y{1}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by '*' matches a sequence of 0 or more matches of the atom.
$t = 'None or more';
$s = 'abbbc';
$p = 'ab*c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /ab*c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'adc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /ab*c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by '+' matches a sequence of 1 or more matches of the atom.
$t = 'One or more';
$s = 'abbbc';
$p = 'ab+c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /ab+c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'ac';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /ab+c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by '?' matches a sequence of 0 or 1 matches of the atom.
$t = 'None or one';
$s = 'abc';
$p = 'ab?c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /ab?c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'adc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /ab?c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## A bound is '{' followed by an unsigned decimal integer, possibly followed 
## by ',' possibly followed by another unsigned decimal integer,
## always followed by '}'.
$t = 'Bound';
$s = 'abcc';
$p = 'a{1}b{0,}c{2,3}';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a{0}b{1,}c{2,3}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'adc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a{0}b{1,}c{2,3}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## The integers must lie between 0 and RE_DUP_MAX (255(!)) inclusive, 
## and if there are two of them, the first may not exceed the second.
$t = 'Bound integers';
$s = 'abcc';
$p = 'b{0,1}c{2,255}';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /b{0,1}c{2,255}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /b{0,1}c{2,255}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by a bound containing one integer
## i and no comma matches a sequence of exactly i matches of the atom.
$t = 'Bound integer exactly';
$s = 'abccd';
$p = 'bc{2}d';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /bc{2}d/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abcccd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /bc{2}d/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by a bound containing one integer i and a comma matches a
## sequence of i or more matches of the atom.
$t = 'Bound integer or more';
$s = 'abccccc';
$p = 'c{2,}';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /c{2,}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abcdc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /c{2,}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom followed by a bound containing two integers i and j matches a sequence 
## of i through j (inclusive) matches of the atom.
$t = 'Bound integer through maximum';
$s = 'abccccd';
$p = 'bc{2,4}d';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /bc{2,4}d/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abcdbcccccd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /bc{2,4}d/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## An atom is a regular expression enclosed in '()' (matching a match for
## the regular expression),
$t = 'Enclosed regex';
$s = 'abc';
$p = '(b)';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /(b)/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'acd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /(b)/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## an empty set of '()' (matching the null string)(!),
$t = 'Enclosed empty';
$s = '';
$p = '^()$';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /^()$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'acd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^()$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
       
## a bracket expression (see below),

## '.'  (matching any single character),
$t = 'any single character';
$s = 'abc';
$p = 'a.c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a.c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'acd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a.c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p); 

## '^' (matching the null string at the beginning of a  line),
$t = 'beginning of line/string';
$s = 'abc';
$p = '^ab';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /^ab/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'acd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^ab/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## '$'  (matching the null string at the end of a line),
$t = 'end of line/string';
$s = 'abc';
$p = 'bc$';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /bc$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'bcd';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /bc$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p); 

## a '\' followed by one of the characters '^.[$()|*+?{\' (matching that 
## character taken as an ordinary character),
$t = 'escaped special character';
$s = '^.[$()|*+?{\\';
$p = '\^\.\[\$\(\)\|\*\+\?\{\\';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /\^\.\[\$\(\)\|\*\+\?\{\\/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = '\^\.\[\$\(\)\|\*\+\?\{\\';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /\^\.\[\$\(\)\|\*\+\?\{\\/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
  
# This seems to be against POSIX
## a '\' followed by any other character(!)
## (matching that character taken as an ordinary character, as if the '\'
## had not been present(!)),
# NOTE: escaping multi-byte characters does not work for some reason (\�)
$t = 'escaped ordinary character';
$s = '!"%&=~#-_>';
# $p = '\a\A\!\"\�\%\&\=\~\#\-\_\>';
$p = '\!\"\%\&\=\~\#\-\_\>';
# -- digits do not work: REGEX-COMPILATION-ERROR: Invalid back reference
# printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /\a\A\1\!\"\%\&\=\~\#\-\_\<\>\,\;/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
# -- with PCRE the paragraph sign seems to be a problem
# | unhandled QORE System exception thrown at ./regex.q:177
# | REGEX-COMPILATION-ERROR: invalid UTF-8 string
# | chained exception:
# | unhandled QORE System exception thrown at ./regex.q:179
# | REGEX-COMPILATION-ERROR: invalid UTF-8 string
# printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /\a\A\!\"\�\%\&\=\~\#\-\_\>/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
# printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /\a\A\!\"\�\%\&\=\~\#\-\_\>/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /\!\"\%\&\=\~\#\-\_\>/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = '\!\"\%\&\=\~\#\-\_\>';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /\!\"\%\&\=\~\#\-\_\>/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

# escaping '<,;}' with '\' has problems
# NOTE: it would be against POSIX 2, actual behaviour is o.k.
$t = 'escaped with problems';
$s = '<,;}';
$p = '\<\,\;\}';
#printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /\<\,\;\}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = '\<\,\;\}';
#printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /\<\,\;\}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);  

## or a single character with no other significance (matching that character).
$t = 'unescaped character';
$s = '<,;}';
$p = '<,;}';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /<,;}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /<,;}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);  

## A '{' followed by a character other than a digit is an ordinary character, 
## not the beginning of a bound(!).
$t = 'unescaped character';
$s = 'a{b}';
$p = 'a{b}';
# NOTE: it would be against POSIX 2, actual behaviour is o.k.
#REGEX-COMPILATION-ERROR: Invalid content of \{\}
#REGEX-COMPILATION-ERROR: Unmatched \{
#printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a{b/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abc';
#printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a{b}/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

       
## It is illegal to end an RE with '\'.

## A bracket expression is a list of characters enclosed in '[]'. It normally  
## matches any single character from the list (but see below).  
$t = 'bracket expressions'; 
$s = 'abc';
$p = 'a[bB]c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a[bB]c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'adc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a[bB]c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## If the list begins with '^', it matches  any  single  character  (but  see
## below) not from the rest of the list.
$t = 'negated bracket list'; 
$s = 'abc';
$p = 'a[^d]c';
printf("%s %s: \"'%s' =~ /%s/\"\n", ( $s =~ /a[^d]c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'adc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /a[^d]c/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
       
## If two characters in the list are separated by '-', this is shorthand for 
## the full range of characters between those two (inclusive) in the collating 
## sequence, e.g. '[0-9]' in ASCII matches any decimal digit.
$t = 'char range';
$s = 'abc';
$p = '^[a-z]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[a-z]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
$s = 'ABC';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[a-z]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## It is illegal(!) for two ranges to share an endpoint, e.g. 'a-c-e'. Ranges are 
## very collating sequence-dependent, and portable programs should avoid relying on them.

## To include a literal ']' in the list, make it the first character (following 
## a possible '^').
$t = 'literal bracket';
$s = 'abc]';
$p = '^[]a-c]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[]a-c]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
$s = 'abc[';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[]a-c]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## To include a literal '-', make it the first 
$t = 'literal hyphen first';
$s = 'abc-';
$p = '^[-a-c]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[-a-c]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
$s = 'abc[';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[-a-c]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## or last character, 
$t = 'literal hyphen last';
$s = 'abc-';
$p = '^[a-c-]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[a-c-]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
$s = 'abc[';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[a-c-]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## or the second endpoint of a range. 
$t = 'hyphen range endpoint';
$s = '!#-';
$p = '^[!--]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[!--]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
$s = 'abc[';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[!--]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## To use a literal '-' as the first endpoint of a range, enclose it in '[.'  and  '.]'  
## to make it a collating element (see below).  
$t = 'hyphen range startpoint';
$s = 'abc-';
$p = '^[--c]+$';
# this is POSIX
# printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[.-.]-c]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);  
# this is PCRE
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[--c]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'abc!';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[--c]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## With the exception of these and some combinations using `[' (see next paragraphs), 
## all other special characters, including '\', lose their special significance within
## a bracket expression.
# NOTE: In Perl-Regex escaping of all characters within a bracket expression is allowed.
$t = 'bracket unescaped';
$s = '.$()|*+?{}\<>';
# $p = '^[.$()|*+?{}\<>]+$'; # POSIX
$p = '^[.$()|*+?{}\\<>]+$';   # PCRE
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[.$()|*+?{}\\<>]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[.$()|*+?{}\\<>]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## Within a bracket expression, a collating element (a character, a multicharacter 
## sequence that collates as if it were a single character, or a collating-sequence 
## name for either) enclosed in '[.'  and  '.]' stands for the sequence of 
## characters of that collating element. The sequence is a single element of the 
## bracket expression's list. A bracket expression containing a multi-character  
## collating element can thus match more than one character, e.g. if the collating 
## sequence includes a 'ch' collating element, then the RE `[[.ch.]]*c' matches the 
## first five characters of 'chchcc'.
$t = 'collating element';
$s = 'abcba';
$p = '^[[.abc.]]+$';
# REGEX-COMPILATION-ERROR: Invalid collation character
# printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[.abc.]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa';
# printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[.abc.]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## Within a bracket expression, a collating element enclosed in '[=' and
## '=]' is an equivalence class, standing for the sequences of characters
## of all collating elements equivalent to that one, including itself.
## (If there are no other equivalent collating elements, the treatment is
## as if the enclosing delimiters were '[.' and '.]'.) For example, if o
## and ^ are the members of an equivalence class, then '[[=o=]]',
## '[[=^=]]', and '[o^]' are all synonymous. An equivalence class may
## not(!) be an endpoint of a range.

## Within a bracket expression, the name of a character class enclosed in
## '[:' and ':]' stands for the list of all characters belonging to that
## class. Standard character class names are:

##       alnum       digit       punct
##       alpha       graph       space
##       blank       lower       upper
##       cntrl       print       xdigit

$t = 'named character class';
$s = 'abc123';
$p = '^[[:alnum:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:alnum:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa.';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:alnum:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = 'abc';
$p = '^[[:alpha:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:alpha:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa.';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:alpha:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = ' ';
$p = '^[[:blank:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:blank:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa.';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:blank:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = "\t\r\n";
$p = '^[[:cntrl:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:cntrl:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa.';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:cntrl:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = '1234567890';
$p = '^[[:digit:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:digit:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abcbaa.';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:digit:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = 'abc';
$p = '^[[:lower:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:lower:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'ABC';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:lower:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = 'abc';
$p = '^[[:print:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:print:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = '';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:print:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = '!?,;.:';
$p = '^[[:punct:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:punct:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abc1';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:punct:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = "\t\r\n ";
$p = '^[[:space:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:space:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abc1';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:space:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = 'ABC';
$p = '^[[:upper:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:upper:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'abc';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:upper:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'named character class';
$s = '0123456789abcdefABCDEF';
$p = '^[[:xdigit:]]+$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[[:xdigit:]]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p); 
$s = 'g';
printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /^[[:xdigit:]]+$/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## These stand for the character classes defined in wctype(3). A locale
## may provide others. A character class may not be used as an endpoint
## of a range.

## There are two  special  cases(!) of bracket expressions: the bracket
## expressions '[[:<:]]' and '[[:>:]]' match the null string at the begin-
## ning  and  end of a word respectively. A word is defined as a sequence
## of word characters which is neither preceded nor followed by word char-
## acters. A word character is an alnum character (as defined by
## wctype(3)) or an underscore. This is an extension, compatible with but
## not specified by POSIX 1003.2, and should be used with caution in soft-
## ware intended to be portable to other systems.

$t = 'Word begin';
$s = ' abcd efg';
$p = '[[:<:]]abc';
# REGEX-COMPILATION-ERROR: Invalid character class name
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /[[:<:]]abc/) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = ' xabcd efg';
#printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /[[:<:]]abc/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'Word end';
$s = ' abcd efg';
$p = 'bcd[[:>:]]';
# REGEX-COMPILATION-ERROR: Invalid character class name
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /bcd[[:>:]]/) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = ' abcdx efg';
#printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /bcd[[:>:]]/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$t = 'Word';
$s = ' abcd efg';
$p = '[[:<:]]abcd[[:>:]]';
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /[[:<:]]abcd[[:>:]]/) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = ' xabcd efg';
#printf("%s %s: \"'%s' !~ /%s/\"\n", ( $s !~ /[[:<:]]abcd[[:>:]]/ ) ? 'PASS' : 'FAIL' ,$t, $s, $p);

## In the event that an RE could match more than one substring of a given
## string, the RE matches the one starting earliest in the string. If the
## RE could match more than one  substring  starting  at  that  point, it
## matches  the  longest. Subexpressions also match the longest possible
## substrings, subject to the constraint that the whole match be as long
## as possible, with subexpressions starting earlier in the RE taking pri-
## ority over ones starting later. Note that higher-level subexpressions
## thus take priority over their lower-level component subexpressions.
       
## Match  lengths  are  measured in characters, not collating elements.  A
## null string is considered longer than no match at  all.   For  example,
## 'bb*' matches the three middle characters of 'abbbc',
## '(wee|week)(knights|nights)' matches all ten characters of
## 'weeknights', when '(.*).*' is matched against 'abc' the parenthesized
## subexpression matches all three characters, and when `(a*)*' is matched
## against 'bc' both the whole RE and the parenthesized subexpression
## match the null string.
       
## If case-independent matching is specified, the effect is much as if all
## case distinctions had vanished from the alphabet. When an alphabetic
## that exists in multiple cases appears as an ordinary character outside
## a bracket expression, it is effectively transformed into a bracket
## expression containing both cases, e.g. 'x' becomes '[xX]'. When it
## appears inside a bracket expression, all case counterparts of it are
## added to the bracket expression, so that (e.g.) '[x]'  becomes '[xX]'
## and '[^x]' becomes '[^xX]'.
       
## No particular limit is imposed on the length of REs(!). Programs
## intended to be portable should not employ REs longer than 256 bytes, as
## an implementation can refuse to accept such REs and remain POSIX-com-
## pliant.
       
## Finally, there is one new type of atom, a back reference: '\' followed 
## by a non-zero decimal digit d matches the same sequence of characters 
## matched by the dth parenthesized subexpression (numbering subexpressions 
## by the positions of their opening parentheses, left to right), so that 
## (e.g.) '\([bc]\)\1' matches 'bb' or 'cc' but not 'bc'.
       


# Metacharacters
    
$t = 'Empty string';
$s = '';
$p = '^$';
printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);
$s = 'a';
printf("%s %s: \"'%s' !~ /%s/\"\n",( $s !~ /^$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);

# Character class

$t = 'char class meta';
$p = '[.;:*+?\/\\\()!"$^,-~{}=&%@]'; # must be escaped: '('->'\(', '\'->'\\'

# This is POSIX and will not work with PCRE
$t = 'Word begin';
$s = ' abcd efg';
$p = '\<abc';
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /\<abc/) ? 'PASS' : 'FAIL' ,$t, $s, $p);

# This is POSIX and will not work with PCRE
$t = 'Word end';
$s = ' abcd efg';
$p = 'bcd\>';
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /bcd\>/) ? 'PASS' : 'FAIL' ,$t, $s, $p);

# This is POSIX and will not work with PCRE
$t = 'Word';
$s = ' abcd efg';
$p = '\<abcd\>';
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /\<abcd\>/) ? 'PASS' : 'FAIL' ,$t, $s, $p);

$s =~ s/\t\n\r\s\S\w\W\d\D//;
$t = 'Symbolic character classes'; # do not work
$s = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789';
$p = '^[\w]+$';
#printf("%s %s: \"'%s' =~ /%s/\"\n",( $s =~ /^[\w]+$/) ? 'PASS' : 'FAIL' ,$t, $s, $p);

# substitution
#$s =~ s/abc/xyz/;
#$s =~ s/abc/xyz/g;
#$s =  'xyz a bc';
#$s =~ s/[a-c]{1,2}/ a /;
#$s =~ s/\(a\)/ a /;
#$s =~ s/\(a\)/ \1 \n/;
$s = 'abc';
#$s =~ s/(a)/ \1 \n/; # does not work
#print($s);


