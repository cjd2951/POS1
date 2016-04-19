#ifndef THREAD_FIXED_POINT_H
#define THREAD_FIXED_POINT_H

#define f (1<<14)

#define int_to_float(n) (n)*(f)

#define float_to_int_zero_round(x) (x)/(f)

#define float_to_int_nearest_round(x) ((x) >= 0 ? ((x)+(f)/2)/(f):((x)-(f)/2)/(f))

#define add_floats(x, y) (x)+y

#define sub_floats(x, y) (x) - y

#define add_float_and_int(x, n) (x) + int_to_float(n)

#define sub_float_and_int(x, n) (x) - int_to_float(n)

#define multi_floats(x, y) ((int64_t)(x))*y/(f)

#define multi_float_and_int(x, n) (x)*(n)

#define divide_floats(x, y) ((int64_t)(x))*(f)/y

#define divide_float_and_int(x, n) (x)/(n)

#endif