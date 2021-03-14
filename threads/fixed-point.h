/* Fixed-Point Real Arithmetic */
#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H


/* 17 bits before decimal point, 14 bits after it.
   1 bit for the sign. */
#define P 17
#define Q 14
#define F 1 << (Q)

#define CONVERT_TO_FIXED_POINT(n)  (n)*(F)      //n is integer
#define CONVERT_TO_INTZERO(x) (x)/(F)           //x is fixed point and signed. This one rounds to 0


//Basic operations between 2 fixed-point numbers
#define ADD(x,y) (x)+(y)                        //x and y are fixed-point numbers
#define SUBS(x,y) (x)-(y) 
#define MULTI(x,y) (x)*(y) 
#define DIV(x,y) (x)/(y)

#define 

#endif