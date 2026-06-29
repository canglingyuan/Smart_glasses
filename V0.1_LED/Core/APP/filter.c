#include "filter.h"

void BubbleSort(float arr[], int n)
{
    // 冒泡排序（从 hcsr04.c 复制过来）
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - 1 - i; j++)
        {
            if (arr[j] > arr[j + 1])
            {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

float MixedFilter(float samples[], int valid_count)
{
    BubbleSort(samples, valid_count);

    int start = 1;
    int end = valid_count - 2;
    float sum = 0.0f;
    for (int i = start; i <= end; i++)
    {
        sum += samples[i];
    }
    return sum / (end - start + 1);
}

float LowPassFilter(float raw, float prev, float alpha)
{
    if (prev < 0) return raw;
    return alpha * raw + (1.0f - alpha) * prev;
}