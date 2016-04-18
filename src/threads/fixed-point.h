#define f (1<<14)


int int_to_float(int n){
    return n*f;
}

int float_to_int_zero_round(int x){
    return x/f;
}

int float_to_int_nearest_round(int x){
    if(x >= 0){
        return (x+f/2)/f;
    }else{
        return (x-f/2)/f;
    }
}

int add_floats(int x, int y){
    return x+y;
}

int sub_floats(int x, int y){
    return x - y;
}

int add_float_and_int(int x, int n){
    return x + int_to_float(n);
}

int sub_float_and_int(int x, int n){
    return x - int_to_float(n);
}

int multi_floats(int x, int y){
    return ((int64_t)x)*y/f;
}

int multi_float_and_int(int x, int n){
    return x*n;
}

int divide_floats(int x, int y){
    return ((int64_t)x)*f/y;
}

int divide_float_and_int(int x, int n){
    return x/n;
}