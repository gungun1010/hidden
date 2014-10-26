performance: 5.619 MPKI

my raw GShare perf is 5.79 MPKI

On top of GShare:
I created a 2-bit table selector based on last 2 BP outcomes. (00,01,10,11), and the selector choses which bpt to choose from. 
So total storage is:
2^2 	*  2^15  *  2 = 32KB
|	    |       |
2-bit     table	   counter bits	 
table     entries
selector