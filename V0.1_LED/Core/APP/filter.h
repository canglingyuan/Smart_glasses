#ifndef __FILTER_H
#define __FILTER_H

#include <stdint.h>

// 对采样数组进行冒泡排序
void BubbleSort(float arr[], int n);

// 中值 + 均值混合滤波（掐头去尾取中值）
float MixedFilter(float samples[], int valid_count);

// 一阶低通滤波
float LowPassFilter(float raw, float prev, float alpha);

#endif