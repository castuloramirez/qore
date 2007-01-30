#!/usr/bin/env qore

%enable-all-warnings
%disable-warning undeclared-var

# examples of qore expressions

# some basic types
$l = ();                    # an empty list
# note that the += operator appends elements to a list

$l += 1;                    # integer
$l += 1.5;                  # floating point number
$l += "string";             # string
$l += 2005-01-05;           # date
$l += 2005-01-01-10:15:45;  # date and time
$l += NOTHING;              # <no value>
$l += NULL;                 # a database NULL -- NOTE: not the same as NOTHING
$l += ( "key" : "value" );   # a hash
 
# print out these values
printf("l=%N\n", $l);

# however, += concatenates to strings, and adds to integers, floating-point numbers, 
# and dates, depending on the data type of the lvalue (on the left-hand side of the +=)
$l[0] += 5;
printf("$l[0]=%n\n", $l[0]);
$l[1] += 5;
printf("$l[1]=%n\n", $l[1]);
$l[2] += " plus 5";
printf("$l[2]=%n\n", $l[2]);
$l[3] += 1Y + 4M + 5D + 4h + 10m + 31s; # adds 1 year, 4 months, 5 days, 4 hours, 10 minutes, and 31 seconds
printf("$l[3]=%n\n", $l[3]);

# note that NOTHING != NULL
printf("NOTHING != NULL is %n\n", NOTHING != NULL);

# regular expression matching
if ("hello how are you" =~ /^hell/)
    print("regular expression match OK\n");

if ("hello how are you" !~ /^xyz/)
    print("regular expression nmatch OK\n");

# regular expression substitution
$l[2] =~ s/tring/ammy/;
printf("regular expression substitution: %n\n", $l[2]);
$l[2] =~ s/m/l/g;
printf("regular expression substitution: %n\n", $l[2]);

# working with hashes
$h = ( "key1" : 1,
       "key2" : "two",
       "key3" : 3.5 );

printf("keys in hash: %n\n", keys $h);
foreach my $k in (keys $h)
    printf("$h.%s = %n\n", $k, $h.$k);

# print formatting supports the usual formatting arguments, plus:
# %n = print out any value (including lists, hashes, and objects) on one line
# %N = same as %n except that complex objects are formatted on different lines
printf("one line hash=%n\n", $h);
printf("multiple line hash=%N\n", $h);

# find expression can find data in a hash of arrays
# (such as returned from the Datasource::select() method

$h = ( "name" : ( "Arnie", "Sylvia", "Carol" ),
       "dob"  : ( 1981-04-23, 1972-11-22, 1995-03-11 ) );
printf("Carol was born on %n\n", find %dob in $h where (%name == "Carol"));

# shift pulls off the first element of a list
printf("shifted element=%n, list=%n\n", shift $l, $l);

# the "exists" operator can tell you if a value exists
# NOTE: exists <expr> is the same as <expr> == NOTHING
if (!exists $notexists)
    printf("exists operator OK\n");

# the comparison operator (==) will return True if the values are equivalent
# but not of the same type, for example
printf("'1' == 1 is %N\n", '1' == 1);

# however the absolute comparison operator (===) requires that the types also be equal
printf("'1' === 1 is %N\n", '1' === 1);
# note that != and !== are the negations of the above two operators

# there is also a C-style question mark operator to do conditional evaluations
# without having to use an "if" statement
printf("question mark: value = %N\n", True ? "string" : "ERROR");

# the background operator starts a new thread, returns the new thread ID to the caller
sub test()
{
    printf("this is TID %d\n", gettid());
}

$tid = background test();
printf("TID %d just started TID %d\n", gettid(), $tid);

# an easy way to execute an external program and capture the output is to use the
# backquote (or backtick) operator (`)
printf("%s", `ls -l *.q`);

# note that the function backquote() does the same thing but allows an expression
# to be used to give the shell command to execute.  Also the system() function 
# executes external programs, but does not return the output
