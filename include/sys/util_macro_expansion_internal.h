/**
 * @file
 * @brief Misc utilities
 *
 * Repetitive or obscure helper macros needed by sys/util_macro_expansion.h.
 */

#ifndef ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_INTERNAL_H_
#define ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sys-util
 * @{
 */

#define IS_0(i)	COND_CODE_0(i, (1), (0))
#define IS_1(i)	COND_CODE_1(i, (1), (0))
#define IS_2(i)	IS_1(DECREMENT_NUM_1(i))
#define IS_3(i)	IS_2(DECREMENT_NUM_1(i))
#define IS_4(i)	IS_3(DECREMENT_NUM_1(i))
#define IS_5(i)	IS_4(DECREMENT_NUM_1(i))
#define IS_6(i)	IS_5(DECREMENT_NUM_1(i))
#define IS_7(i)	IS_6(DECREMENT_NUM_1(i))
#define IS_8(i)	IS_7(DECREMENT_NUM_1(i))
#define IS_9(i)	IS_8(DECREMENT_NUM_1(i))
#define IS_10(i)	IS_9(DECREMENT_NUM_1(i))
#define IS_11(i)	IS_10(DECREMENT_NUM_1(i))
#define IS_12(i)	IS_11(DECREMENT_NUM_1(i))
#define IS_13(i)	IS_12(DECREMENT_NUM_1(i))
#define IS_14(i)	IS_13(DECREMENT_NUM_1(i))
#define IS_15(i)	IS_14(DECREMENT_NUM_1(i))
#define IS_16(i)	IS_15(DECREMENT_NUM_1(i))
#define IS_N_(N, i) IS_##N(i)



/* Set of UTIL_LISTIFY particles for nesting level 1 */
#define Z_UTIL_LISTIFY_1_0(F, ...)

#define Z_UTIL_LISTIFY_1_1(F, ...) \
	F(0, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_2(F, ...) \
	Z_UTIL_LISTIFY_1_1(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_3(F, ...) \
	Z_UTIL_LISTIFY_1_2(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_4(F, ...) \
	Z_UTIL_LISTIFY_1_3(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_5(F, ...) \
	Z_UTIL_LISTIFY_1_4(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_6(F, ...) \
	Z_UTIL_LISTIFY_1_5(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_7(F, ...) \
	Z_UTIL_LISTIFY_1_6(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_8(F, ...) \
	Z_UTIL_LISTIFY_1_7(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_9(F, ...) \
	Z_UTIL_LISTIFY_1_8(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_10(F, ...) \
	Z_UTIL_LISTIFY_1_9(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_11(F, ...) \
	Z_UTIL_LISTIFY_1_10(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_12(F, ...) \
	Z_UTIL_LISTIFY_1_11(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_13(F, ...) \
	Z_UTIL_LISTIFY_1_12(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_14(F, ...) \
	Z_UTIL_LISTIFY_1_13(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_15(F, ...) \
	Z_UTIL_LISTIFY_1_14(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_1_16(F, ...) \
	Z_UTIL_LISTIFY_1_15(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

/* Set of UTIL_LISTIFY particles for nesting level 2 */
#define Z_UTIL_LISTIFY_2_0(F, ...)

#define Z_UTIL_LISTIFY_2_1(F, ...) \
	F(0, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_2(F, ...) \
	Z_UTIL_LISTIFY_2_1(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_3(F, ...) \
	Z_UTIL_LISTIFY_2_2(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_4(F, ...) \
	Z_UTIL_LISTIFY_2_3(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_5(F, ...) \
	Z_UTIL_LISTIFY_2_4(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_6(F, ...) \
	Z_UTIL_LISTIFY_2_5(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_7(F, ...) \
	Z_UTIL_LISTIFY_2_6(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_8(F, ...) \
	Z_UTIL_LISTIFY_2_7(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_9(F, ...) \
	Z_UTIL_LISTIFY_2_8(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_10(F, ...) \
	Z_UTIL_LISTIFY_2_9(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_11(F, ...) \
	Z_UTIL_LISTIFY_2_10(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_12(F, ...) \
	Z_UTIL_LISTIFY_2_11(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_13(F, ...) \
	Z_UTIL_LISTIFY_2_12(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_14(F, ...) \
	Z_UTIL_LISTIFY_2_13(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_15(F, ...) \
	Z_UTIL_LISTIFY_2_14(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_2_16(F, ...) \
	Z_UTIL_LISTIFY_2_15(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)


/* Set of UTIL_LISTIFY particles for nesting level 1 */
#define Z_UTIL_LISTIFY_3_0(F, ...)

#define Z_UTIL_LISTIFY_3_1(F, ...) \
	F(0, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_2(F, ...) \
	Z_UTIL_LISTIFY_3_1(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_3(F, ...) \
	Z_UTIL_LISTIFY_3_2(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_4(F, ...) \
	Z_UTIL_LISTIFY_3_3(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_5(F, ...) \
	Z_UTIL_LISTIFY_3_4(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_6(F, ...) \
	Z_UTIL_LISTIFY_3_5(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_7(F, ...) \
	Z_UTIL_LISTIFY_3_6(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_8(F, ...) \
	Z_UTIL_LISTIFY_3_7(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_9(F, ...) \
	Z_UTIL_LISTIFY_3_8(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_10(F, ...) \
	Z_UTIL_LISTIFY_3_9(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_11(F, ...) \
	Z_UTIL_LISTIFY_3_10(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_12(F, ...) \
	Z_UTIL_LISTIFY_3_11(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_13(F, ...) \
	Z_UTIL_LISTIFY_3_12(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_14(F, ...) \
	Z_UTIL_LISTIFY_3_13(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_15(F, ...) \
	Z_UTIL_LISTIFY_3_14(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_3_16(F, ...) \
	Z_UTIL_LISTIFY_3_15(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)


/* Set of UTIL_LISTIFY particles for nesting level 1 */
#define Z_UTIL_LISTIFY_4_0(F, ...)

#define Z_UTIL_LISTIFY_4_1(F, ...) \
	F(0, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_2(F, ...) \
	Z_UTIL_LISTIFY_4_1(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_3(F, ...) \
	Z_UTIL_LISTIFY_4_2(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_4(F, ...) \
	Z_UTIL_LISTIFY_4_3(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_5(F, ...) \
	Z_UTIL_LISTIFY_4_4(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_6(F, ...) \
	Z_UTIL_LISTIFY_4_5(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_7(F, ...) \
	Z_UTIL_LISTIFY_4_6(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_8(F, ...) \
	Z_UTIL_LISTIFY_4_7(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_9(F, ...) \
	Z_UTIL_LISTIFY_4_8(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_10(F, ...) \
	Z_UTIL_LISTIFY_4_9(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_11(F, ...) \
	Z_UTIL_LISTIFY_4_10(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_12(F, ...) \
	Z_UTIL_LISTIFY_4_11(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_13(F, ...) \
	Z_UTIL_LISTIFY_4_12(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_14(F, ...) \
	Z_UTIL_LISTIFY_4_13(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_15(F, ...) \
	Z_UTIL_LISTIFY_4_14(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)

#define Z_UTIL_LISTIFY_4_16(F, ...) \
	Z_UTIL_LISTIFY_4_15(F, __VA_ARGS__) \
	F(1, __VA_ARGS__)



#define UTIL_COND_CHOICE_2(C1, C2, R1, R2)		\
	COND_CODE_1(C1,	R1, ())				\
	COND_CODE_1(C2,	R2, ())

#define UTIL_COND_CHOICE_3(C1, C2, C3, R1, R2, R3)	\
	UTIL_COND_CHOICE_2(C1, C2, R1, R2)	\
	COND_CODE_1(C3,	R3, ())

#define UTIL_COND_CHOICE_4(C1, C2, C3, C4, R1, R2, R3, R4)	\
	UTIL_COND_CHOICE_3(C1, C2, C3, R1, R2, R3)	\
	COND_CODE_1(C4,	R4, ())

#define UTIL_COND_CHOICE_5(C1, C2, C3, C4, C5, R1, R2, R3, R4, R5)	\
	UTIL_COND_CHOICE_4(C1, C2, C3, C4, R1, R2, R3, R4)	\
	COND_CODE_1(C5,	R5, ())

#define UTIL_COND_CHOICE_6(C1, C2, C3, C4, C5, C7, R1, R2, R3, R4, R5, R6)	\
	UTIL_COND_CHOICE_5(C1, C2, C3, C4, C5, R1, R2, R3, R4, R5)	\
	COND_CODE_1(C6,	R6, ())

#define UTIL_COND_CHOICE_7(C1, C2, C3, C4, C5, C6, C7, R1, R2, R3, R4, R5, R6, R7)	\
	UTIL_COND_CHOICE_6(C1, C2, C3, C4, C5, C6, R1, R2, R3, R4, R5, R6)	\
	COND_CODE_1(C7,	R7, ())



#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_UTIL_MACROS_EXPANSION_INTERNAL_H_ */
