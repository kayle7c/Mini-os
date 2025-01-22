#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

//位图初始化
void bitmap_init(struct bitmap* btmp)
{
    memset(btmp->bits,0,btmp->btmp_bytes_len);
}

//检查位图某一位
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx)
{
    return btmp->bits[bit_idx/8] & (1<<bit_idx%8);
}

//在位图中连续申请n位
int bitmap_scan(struct bitmap* btmp, uint32_t cnt)
{
    uint32_t idx_byte=0;
    while((0xff == btmp->bits[idx_byte]) && idx_byte < btmp->btmp_bytes_len)
    {
        idx_byte++;
    }

    ASSERT(idx_byte < btmp->btmp_bytes_len);
    if(idx_byte == btmp->btmp_bytes_len)   //位图已满
        return -1;

    int idx_bit=0;
    while((uint8_t)(1 << idx_bit) & btmp->bits[idx_byte]) //找到第一个空闲位
    {
        idx_bit++;
    }

    int bit_idx_start = idx_byte * 8 + idx_bit;
    if(cnt==1)
    {
        return bit_idx_start;
    }

    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);   // 记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;	      // 用于记录找到的空闲位的个数

    bit_idx_start = -1;
    while(bit_left-- > 0)
    {
        if(!(bitmap_scan_test(btmp,next_bit)))//下一位为0
        {
            count++;
        }
        else
        {
            count = 0;
        }
        if(count == cnt)
        {
            bit_idx_start = next_bit - cnt +1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value)
{
    ASSERT((value==0) || (value==1));
    if(value)
    {
        btmp->bits[bit_idx/8] |= (1<<bit_idx%8);
    }
    else
    {
        btmp->bits[bit_idx/8] &= ~(1<<bit_idx%8);
    }
}