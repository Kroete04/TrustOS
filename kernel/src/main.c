
#include "graphics.h"
#include "interrupts.h"
#include "pmm.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>


__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

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
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

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

void kmain(void) {
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

    graphics_init(fb);
    graphics_clear(10, 15, 25);

    graphics_print("TRUSTOS V0.5\n", 0, 255, 130);
    graphics_print("DON'T VERIFY. JUST TRUST.\n\n", 180, 180, 180);
    graphics_print("[OK] KERNEL INITIALIZED\n", 0, 255, 130);
    graphics_print("[OK] FRAMEBUFFER INITIALIZED\n", 0, 255, 130);
    graphics_print("[OK] SUPPORTING GERKMANN <3>\n", 0, 255, 130);
    graphics_print("[OK] SCAMMING USERS LMAO :D>\n", 0, 255, 130);
    graphics_print("[OK] MEETING AT ARCI-STREET>\n", 0, 255, 130);

    interrupts_init();

    graphics_print("[OK] EXCEPTION HANDLERS READY\n", 0, 255, 130);

    if (memmap_request.response == NULL ||
        hhdm_request.response == NULL) {

        graphics_print("[ERROR] MEMORY MAP OR HHDM MISSING\n",255, 60, 60);
        hcf();
    }

    struct limine_memmap_response *memmap =
        memmap_request.response;

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

    graphics_print("\n[OK] MEMORY MAP RECEIVED\n",0, 255, 130);

    graphics_print("MEMORY REGIONS: ", 255, 255, 255);
    print_uint64(memmap->entry_count);

    graphics_print("\nUSABLE REGIONS: ", 255, 255, 255);
    print_uint64(usable_regions);

    graphics_print("\nUSABLE RAM: ", 255, 255, 255);
    print_uint64(usable_bytes / (1024 * 1024));
    graphics_print(" MiB\n", 255, 180, 60);

    graphics_print("[OK] HHDM AVAILABLE\n", 0, 255, 130);

    
    pmm_init(memmap, hhdm_request.response->offset);

    graphics_print("\nINITIAL FREE PAGES: ", 255, 255, 255);
    print_uint64(pmm_get_free_pages());
    
    uint64_t page1 = pmm_alloc_page();
    uint64_t page2 = pmm_alloc_page();

    graphics_print("\nPAGE 1 ADDRESS: ", 255, 255, 255);
    print_uint64(page1);

    graphics_print("\nPAGE 2 ADDRESS: ", 255, 255, 255);
    print_uint64(page2);

    graphics_print("\n", 255, 255, 255);

    if (page1 == 0 || page2 == 0 || page1 == page2) {
     graphics_print("\n[ERROR] PMM ALLOCATION FAILED\n",255, 60, 60);
        hcf();
    }

    graphics_print("\n[OK] ALLOCATED TWO PAGES\n", 0, 255, 130);

    graphics_print("FREE PAGES AFTER ALLOCATION: ",255, 255, 255);
    print_uint64(pmm_get_free_pages());

    pmm_free_page(page1);
    pmm_free_page(page2);

    graphics_print("\nFREE PAGES AFTER FREE: ",255, 255, 255);
    print_uint64(pmm_get_free_pages());

    graphics_print("\n[OK] PMM TEST COMPLETE\n",0, 255, 130);

    hcf();

}