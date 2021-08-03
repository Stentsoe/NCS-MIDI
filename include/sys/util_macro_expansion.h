/**
 * @file
 * @brief Macro utilities
 *
 * Macro utilities are the public interface for C/C++ code and device tree
 * related implementation.  In general, C/C++ will include <sys/util.h>
 * instead this file directly.  For device tree implementation, this file
 * should be include instead <sys/util_internal.h>
 */

#ifndef ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_H_
#define ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sys-util
 * @{
 */

#include "sys/util_macro_expansion_internal.h"

/** 
 * @brief Helper to make sure N is expanded before it is evaluated
 */
#define GET_ARG_N_EXPANDED(N, ...)	GET_ARG_N(N, __VA_ARGS__)

/** @brief Adds a comma after elemnet i, Compatible with  @ref UTIL_LISTIFY.
 *  Which makes it useful for making lists.
 */
#define LIST(i, _)	i, 

/** 
 * @brief Create a list of N numbers counting upwards from zero.
 */
#define UTIL_INCREMENTAL_LIST(N)	LIST_DROP_EMPTY(UTIL_LISTIFY(N, LIST))

/** 
 * @brief Same as @ref UTIL_LISTIFY(), but with ability for listing at multiple nested levels.
 */
#define UTIL_LISTIFY_LEVEL_1(LEN, F, ...) UTIL_CAT(Z_UTIL_LISTIFY_1_, LEN)(F, __VA_ARGS__)
#define UTIL_LISTIFY_LEVEL_2(LEN, F, ...) UTIL_CAT(Z_UTIL_LISTIFY_2_, LEN)(F, __VA_ARGS__)
#define UTIL_LISTIFY_LEVEL_3(LEN, F, ...) UTIL_CAT(Z_UTIL_LISTIFY_3_, LEN)(F, __VA_ARGS__)
#define UTIL_LISTIFY_LEVEL_4(LEN, F, ...) UTIL_CAT(Z_UTIL_LISTIFY_4_, LEN)(F, __VA_ARGS__)
#define UTIL_LISTIFY_LEVEL_5(LEN, F, ...) UTIL_CAT(Z_UTIL_LISTIFY_5_, LEN)(F, __VA_ARGS__)

/** 
 * @brief Increment a number by one
 */
#define	INCREMENT_NUM_1(num)	\
	NUM_VA_ARGS_LESS_1(UTIL_LISTIFY_LEVEL_4(num, LIST),)

/** 
 * @brief Decrement a number by one
 */
#define	DECREMENT_NUM_1(num)	\
	NUM_VA_ARGS_LESS_1(LIST_DROP_EMPTY(UTIL_LISTIFY_LEVEL_4(num, LIST)))

/** 
 * @brief Check if a number i is equal to N. N has to be 16 or lower.
 */
#define IS_N(N, i) IS_N_(N, i)

/** 
 * @brief Expands to Result n (Rn) if Choice n (Cn) expands to 1. N sets the numbers 
 * of Choices to evaluate (max 7). Number of aguments must equal N choices and N results, 
 * where choices are listed first, and results second. Results must be passed in brackets. 
 * 
 * 	Example:
 * 		#define C1 IS_N(3, 3)
 * 		#define C2 IS_N(2, 0)
 * 		#define C2 IS_N(5, 5)
 * 		#define R1 a ,
 * 		#define R2 b ,
 * 		#define R3 c ,
 * 
 * 		UTIL_COND_CHOICE_N(3, C1, C2, C3, (R1), (R2), (R3)) 
 * 
 * 		Expands to: a , c ,
 * 
 * This macro is useful for enums where only one choice is known 
 * to be valid, and therefore only one result will be returned. 
 * However as illustrated in the example all choices that return 1
 * will cause its result to be returned.
 */
#define UTIL_COND_CHOICE_N(N, ...)	UTIL_CAT(UTIL_COND_CHOICE_, N)(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_H_ */
