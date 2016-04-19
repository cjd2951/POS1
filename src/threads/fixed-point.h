#ifndef THREAD_FIXED_POINT_H
#define THREAD_FIXED_POINT_H

#define f (1<<14)

static int int_to_float(int n);
static int float_to_int_zero_round(int x);
static int float_to_int_nearest_round(int x);
static int add_floats(int x, int y);
static int sub_floats(int x, int y);
static int add_float_and_int(int x, int n);
static int sub_float_and_int(int x, int n);
static int multi_floats(int x, int y);
static int multi_float_and_int(int x, int n);
static int divide_floats(int x, int y);
static int divide_float_and_int(int x, int n);

static int int_to_float(int n){
    return n*f;
}

static int float_to_int_zero_round(int x){
    return x/f;
}

static int float_to_int_nearest_round(int x){
    if(x >= 0){
        return (x+f/2)/f;
    }else{
        return (x-f/2)/f;
    }
}

static int add_floats(int x, int y){
    return x+y;
}

static int sub_floats(int x, int y){
    return x - y;
}

static int add_float_and_int(int x, int n){
    return x + int_to_float(n);
}

static int sub_float_and_int(int x, int n){
    return x - int_to_float(n);
}

static int multi_floats(int x, int y){
    return ((int64_t)x)*y/f;
}

static int multi_float_and_int(int x, int n){
    return x*n;
}

static int divide_floats(int x, int y){
    return ((int64_t)x)*f/y;
}

static int divide_float_and_int(int x, int n){
    return x/n;
}

#endif