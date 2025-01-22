#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"

#define PG_SIZE 4096

/***************  位图地址 ********************/
#define MEM_BITMAP_BASE 0xc009a000
//内核所使用的堆空间起始虚拟地址
#define K_HEAP_START 0xc0100000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)


//物理内存池
struct pool
{
   struct bitmap pool_bitmap;	 // 本内存池用到的位图结构,用于管理物理内存
   uint32_t phy_addr_start;	 // 本内存池所管理物理内存的起始地址
   uint32_t pool_size;		 // 本内存池字节容量
};

struct pool kernel_pool, user_pool;      // 内核内存池和用户内存池
struct virtual_addr kernel_vaddr;	 // 此结构是用来给内核分配虚拟地址

static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
    int vaddr_start=0,bit_index_start=-1;
    uint32_t cnt=0;
    if(pf == PF_KERNEL)
    {
        bit_index_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
        if(bit_index_start == -1)
        {
            return NULL;
        }
        while(cnt < pg_cnt)
        {
            bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_index_start+cnt++,1);
        }
        vaddr_start = kernel_vaddr.vaddr_start+bit_index_start*PG_SIZE;
    }
    else
    {

    }
    return (void*)vaddr_start;
}

/* 得到虚拟地址vaddr对应的pte指针*/
uint32_t* pte_ptr(uint32_t vaddr) {
   /* 先访问到页表自己 + \
    * 再用页目录项pde(页目录内页表的索引)做为pte的索引访问到页表 + \
    * 再用pte的索引做为页内偏移*/
    uint32_t* pte = (uint32_t*)(0xffc00000 + \
    ((vaddr & 0xffc00000) >> 10) + \
	PTE_IDX(vaddr) * 4);
    return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针 */
uint32_t* pde_ptr(uint32_t vaddr) {
   /* 0xfffff是用来访问到页表本身所在的地址 */
   uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
   return pde;
}

//在m_pool指向的物理内存池中分配1个物理页
static void* palloc(struct pool* m_pool)
{
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
    if(bit_idx == -1)
    {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap,bit_idx,1);
    uint32_t page_phyaddr = ((bit_idx*PG_SIZE)+m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}    
//添加虚拟地址和物理地址的映射，所谓映射就是把物理地址+属性填在页表中
static void page_table_add(void* _vaddr, void* _page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);

     /* 先在页目录内判断目录项的P位，若为1,则表示该表已存在 */
    if(*pde & 0x00000001)
    {
        ASSERT(!(*pte & 0x00000001));
        if(!(*pte & 0x00000001))
        {
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
        else
        {
            PANIC("pte repeat");
	        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);     
        }
    }
    else  // 页目录项不存在,所以要先创建页目录再创建页表项.
    {
        /* 页表中用到的页框一律从内核空间分配 */
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
    }
}

/* 分配pg_cnt个页空间,成功则返回起始虚拟地址,失败时返回NULL */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) 
{
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
/***********   malloc_page的原理是三个动作的合成:   ***********
      1通过vaddr_get在虚拟内存池中申请虚拟地址
      2通过palloc在物理内存池中申请物理页
      3通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
***************************************************************/
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) 
    {
      return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以逐个做映射*/
    while (cnt-- > 0) 
    {
        void* page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == NULL) // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
        {  
	        return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr); // 在页表中做映射 
        vaddr += PG_SIZE;		 // 下一个虚拟页
   }
   return vaddr_start;
}

/* 从内核物理内存池中申请pg_cnt页内存,成功则返回其虚拟地址,失败则返回NULL */
void* get_kernel_pages(uint32_t pg_cnt) {
   void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
   if (vaddr != NULL) {	   // 若分配的地址不为空,将页框清0后返回
      memset(vaddr, 0, pg_cnt * PG_SIZE);
   }
   return vaddr;
}

static void mem_pool_init(uint32_t all_mem) 
{
    put_str("   mem_pool_init start\n");
    uint32_t page_table_size = PG_SIZE * 256;	  // 页表大小= 1页的页目录表+第0和第768个页目录项指向同一个页表+
                                                  // 第769~1022个页目录项共指向254个页表,共256个页框
    uint32_t used_mem = page_table_size+0x100000;  // 0x100000为低端1M内存
    uint32_t free_mem = all_mem-used_mem;
    uint16_t all_free_pages = free_mem/PG_SIZE;		  // 1页为4k,不管总内存是不是4k的倍数,
								  // 对于以页为单位的内存分配策略，不足1页的内存不用考虑了。
    uint16_t kernel_free_pages = all_free_pages/2;
    uint16_t user_free_pages = all_free_pages-kernel_free_pages;
    uint32_t kbm_length = kernel_free_pages/8;		  // Kernel BitMap的长度,位图中的一位表示一页,以字节为单位
    uint32_t ubm_length = user_free_pages/8;		  // User BitMap的长度.

    uint32_t kp_start = used_mem; 			  // Kernel Pool start,内核内存池的起始地址
    uint32_t up_start = kp_start+kernel_free_pages*PG_SIZE;		  // User Pool start,用户内存池的起始地址
    
    kernel_pool.phy_addr_start = kp_start;
    user_pool.phy_addr_start   = up_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size	 = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len	  = ubm_length;

//位图
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);
    put_str("      kernel_pool_bitmap_start:");put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("      user_pool_bitmap_start:");put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");put_int(user_pool.phy_addr_start);
    put_str("\n");

    /* 初始化位图为0*/
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    /* 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。*/
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;      // 用于维护内核堆的虚拟地址,所以要和内核内存池大小一致

    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vaddr.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vaddr.vaddr_bitmap);

    put_str("   mem_pool_init done\n");
}

void mem_init()
{
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); //0xb00是在loader定义的存放系统内存总量的地址
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}