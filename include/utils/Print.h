//
// More about variadic macros here: https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
//

#ifndef PRINT_H
#define PRINT_H

#include <iostream>
// variadic print macro
#define PRINT(format, ...) \
	if (format) \
	fprintf(stderr, format, __VA_ARGS__)

#endif // !PRINT_H
