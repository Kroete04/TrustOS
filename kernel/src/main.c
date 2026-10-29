
#include "drivers/video/graphics.h"
#include "arch/x86_64/interrupts.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "arch/x86_64/paging.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// ============================================================
// Limine Requests
// ============================================================

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

// ============================================================
// Kernel Halt
// ============================================================

static void hcf(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

// ============================================================
// Print unsigned 64-bit integer
// ============================================================

static void print_uint64(uint64_t value) {
    char buffer[21];
    size_t i = sizeof(buffer) - 1;

    buffer[i] = '\0';

    do {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    graphics_print(&buffer[i], 255, 180, 60);
}

// ============================================================
// Kernel Main
// ============================================================

void kmain(void) {

    // --------------------------------------------------------
    // 1. Check Limine
    // --------------------------------------------------------

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        hcf();
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *fb =
        framebuffer_request.response->framebuffers[0];

    if (fb->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        fb->bpp != 32) {
        hcf();
    }

    // --------------------------------------------------------
    // 2. Initialize Graphics
    // --------------------------------------------------------

    graphics_init(fb);
    graphics_clear(10, 15, 25);

    graphics_print("TRUSTOS V0.5.1\n", 0, 255, 130);
    graphics_print("DON'T VERIFY. JUST TRUST.\n\n",
                   180, 180, 180);

    graphics_print("[OK] KERNEL INITIALIZED\n",
                   0, 255, 130);

    graphics_print("[OK] FRAMEBUFFER INITIALIZED\n",
                   0, 255, 130);

    graphics_print("[OK] SUPPORTING GERKMANN <3>\n",
                   0, 255, 130);

    graphics_print("[OK] SCAMMING USERS LMAO :D>\n",
                   0, 255, 130);

    graphics_print("[OK] MEETING AT ARCI-STREET>\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 3. Initialize Interrupts
    // --------------------------------------------------------

    interrupts_init();

    graphics_print("[OK] EXCEPTION HANDLERS READY\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 4. Check Memory Map and HHDM
    // --------------------------------------------------------

    if (memmap_request.response == NULL ||
        hhdm_request.response == NULL) {

        graphics_print(
            "\n[ERROR] MEMORY MAP OR HHDM MISSING\n",
            255, 60, 60
        );

        hcf();
    }

    struct limine_memmap_response *memmap =
        memmap_request.response;

    uint64_t hhdm = hhdm_request.response->offset;

    uint64_t usable_bytes = 0;
    uint64_t usable_regions = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry =
            memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            usable_bytes += entry->length;
            usable_regions++;
        }
    }

    graphics_print("\n[OK] MEMORY MAP RECEIVED\n",
                   0, 255, 130);

    graphics_print("MEMORY REGIONS: ", 255, 255, 255);
    print_uint64(memmap->entry_count);

    graphics_print("\nUSABLE REGIONS: ", 255, 255, 255);
    print_uint64(usable_regions);

    graphics_print("\nUSABLE RAM: ", 255, 255, 255);
    print_uint64(usable_bytes / (1024 * 1024));
    graphics_print(" MiB\n", 255, 180, 60);

    graphics_print("[OK] HHDM AVAILABLE\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 5. Initialize Physical Memory Manager
    // --------------------------------------------------------

    if (!pmm_init(memmap, hhdm)) {
        graphics_print("\n[ERROR] PMM INIT FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("\n[OK] PMM INITIALIZED\n",
                   0, 255, 130);

    uint64_t initial = pmm_get_free_pages();

    graphics_print("INITIAL FREE PAGES: ",
                   255, 255, 255);
    print_uint64(initial);

    // --------------------------------------------------------
    // 6. Allocate Two Physical Pages
    // --------------------------------------------------------

    uint64_t page1 = pmm_alloc_page();
    uint64_t page2 = pmm_alloc_page();

    graphics_print("\nPAGE 1 ADDRESS: ", 255, 255, 255);
    print_uint64(page1);

    graphics_print("\nPAGE 2 ADDRESS: ", 255, 255, 255);
    print_uint64(page2);

    graphics_print("\n", 255, 255, 255);

    if (page1 == 0 || page2 == 0 || page1 == page2) {
        graphics_print("[ERROR] ALLOCATION FAILED\n",
                       255, 60, 60);
        hcf();
    }

    if (pmm_get_free_pages() != initial - 2) {
        graphics_print("[ERROR] PAGE COUNT INCORRECT\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] TWO PAGES ALLOCATED\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 7. Test Physical Memory Read / Write
    // --------------------------------------------------------

    volatile uint64_t *memory1 =
        (volatile uint64_t *)(uintptr_t)(page1 + hhdm);

    volatile uint64_t *memory2 =
        (volatile uint64_t *)(uintptr_t)(page2 + hhdm);

    *memory1 = 0x123456789ABCDEF0ULL;
    *memory2 = 0xDEADBEEFCAFEBABEULL;

    if (*memory1 != 0x123456789ABCDEF0ULL ||
        *memory2 != 0xDEADBEEFCAFEBABEULL) {

        graphics_print("[ERROR] MEMORY TEST FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] PHYSICAL MEMORY READ/WRITE\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 8. Free Pages and Test Safety
    // --------------------------------------------------------

    bool free1 = pmm_free_page(page1);
    bool free2 = pmm_free_page(page2);

    // Must fail: page1 was already freed
    bool double_free = pmm_free_page(page1);

    // Must fail: physical address zero is reserved
    bool invalid_free = pmm_free_page(0);

    if (!free1 || !free2 ||
        double_free || invalid_free ||
        pmm_get_free_pages() != initial) {

        graphics_print("[ERROR] PMM SAFETY TEST FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] DOUBLE FREE REJECTED\n",
                   0, 255, 130);

    graphics_print("[OK] INVALID FREE REJECTED\n",
                   0, 255, 130);

    graphics_print("[OK] PAGE COUNTER RESTORED\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 9. PMM Test Complete
    // --------------------------------------------------------

    graphics_print("\nTRUSTOS PMM V0.5.1 READY!\n",
                   0, 255, 130);

    // --------------------------------------------------------
    // 10. Paging Test (v0.6)
    // --------------------------------------------------------

    graphics_clear(10, 15, 25);

    graphics_print("TRUSTOS V0.6 - PAGING\n\n",
                   0, 255, 130);

    uint64_t cr3 = paging_get_cr3();
    uint64_t pml4 = paging_get_pml4_address();

    graphics_print("[OK] PAGING INFORMATION READ\n\n",
                   0, 255, 130);

    graphics_print("CR3: ", 255, 255, 255);
    print_uint64(cr3);

    graphics_print("\nPML4 PHYSICAL ADDRESS: ",
                   255, 255, 255);
    print_uint64(pml4);

    // --------------------------------------------------------
    // 11. Inspect PML4 Entries
    // --------------------------------------------------------

    // Convert the physical PML4 address to a virtual address
    // using the Higher Half Direct Map

    volatile uint64_t *pml4_table =
        (volatile uint64_t *)(uintptr_t)(pml4 + hhdm);

    uint64_t present_entries = 0;

    // A PML4 contains 512 entries
    // Bit 0 indicates whether an entry is present

    for (uint64_t i = 0; i < 512; i++) {
        if (pml4_table[i] & 1ULL) {
            present_entries++;
        }
    }

    graphics_print("\n\nPRESENT PML4 ENTRIES: ",
                   255, 255, 255);
    print_uint64(present_entries);

    graphics_print("\n[OK] PML4 INSPECTION COMPLETE\n",
                   0, 255, 130);
    
    // --------------------------------------------------------
    // 12. VMM Test
    // --------------------------------------------------------

    graphics_clear(10, 15, 25);

    graphics_print("TRUSTOS V0.6 - VMM TEST\n\n",
                   0, 255, 130);

    if (!vmm_init(hhdm)) {
        graphics_print("[ERROR] VMM INIT FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] VMM INITIALIZED\n",
                   0, 255, 130);

    uint64_t test_physical = pmm_alloc_page();

    if (test_physical == 0) {
        graphics_print("[ERROR] PMM ALLOCATION FAILED\n",
                       255, 60, 60);
        hcf();
    }

    // Test address: must not overlap existing mappings
    const uint64_t test_virtual = 0x0000400000000000ULL;

    if (!vmm_map_page(
        test_virtual,
        test_physical,
        VMM_WRITABLE | VMM_NX
    )) {
        graphics_print("[ERROR] VMM MAPPING FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] VIRTUAL PAGE MAPPED\n",
                   0, 255, 130);

    volatile uint64_t *virtual_memory =
        (volatile uint64_t *)(uintptr_t)test_virtual;

    volatile uint64_t *physical_memory =
        (volatile uint64_t *)(uintptr_t)(
            test_physical + hhdm
        );

    *virtual_memory = 0xCAFEBABE12345678ULL;

    if (*physical_memory != 0xCAFEBABE12345678ULL) {
        graphics_print("[ERROR] VMM MEMORY TEST FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] VIRTUAL MEMORY READ/WRITE\n",
                   0, 255, 130);

    if (!vmm_unmap_page(test_virtual)) {
        graphics_print("[ERROR] VMM UNMAP FAILED\n",
                       255, 60, 60);
        hcf();
    }

    if (!pmm_free_page(test_physical)) {
        graphics_print("[ERROR] PMM FREE FAILED\n",
                       255, 60, 60);
        hcf();
    }

    graphics_print("[OK] VIRTUAL PAGE UNMAPPED\n",
                   0, 255, 130);

    graphics_print("\nTRUSTOS VMM TEST COMPLETE!\n",
                   0, 255, 130);

    hcf();

}