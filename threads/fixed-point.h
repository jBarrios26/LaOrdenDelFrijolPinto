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
//(x + f / 2) / f if x >= 0, (x - f / 2) / f if x <= 0. 
#define CONVERT_TO_INT_NEAREST(x) ((x) >= 0 ? ((x) + (F) / 2)/ (F) : ((x) - (F) / 2) / (F))             

//Basic operations between 2 fixed-point numbers
#define ADD(x,y) (x)+(y)                        //x and y are fixed-point numbers
#define SUBS(x,y) (x)-(y) 
#define MULTI(x,y) ((int64_t)(x))*(y)/(F)       //fixed the corrimiento
#define DIV(x,y) ((int64_t)(x))*(F)/(y)         //fixed the corrimiento

//Basic operations between a fixed-point number and a integer
#define ADD_FP_INT(x,n) (x)+ ((n)*(F))             //It multiplies by F to convert it to a fixed-point as well
#define SUB__FP_INT(x,n) (x)-((n)*(F))
#define MULTI_FP_INT(x,n) (x)*(n)
#define DIV_FP_INT(x,n) (x)/(n)

#endif