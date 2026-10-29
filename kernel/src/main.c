
#include "graphics.h"
#include "interrupts.h"

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

    graphics_print("TRUSTOS V0.4\n", 0, 255, 130);
    graphics_print("DON'T VERIFY. JUST TRUST.\n\n", 180, 180, 180);
    graphics_print("[OK] KERNEL INITIALIZED\n", 0, 255, 130);
    graphics_print("[OK] FRAMEBUFFER INITIALIZED\n", 0, 255, 130);
    graphics_print("[OK] SUPPORTING GERKMANN <3>\n", 0, 255, 130);
    graphics_print("[OK] SCAMMING USERS LMAO :D>\n", 0, 255, 130);
    graphics_print("[OK] MEETING AT ARCI-STREET>\n", 0, 255, 130);

    interrupts_init();

    graphics_print("[OK] EXCEPTION HANDLERS READY\n", 0, 255, 130);
    graphics_print("\nTRIGGERING INVALID OPCODE...\n", 255, 180, 60);

    // x86-64: test
    __asm__ volatile("ud2");

    // Should never happen (Hopefully lmao)
    graphics_print("ERROR: EXCEPTION RETURNED!\n", 255, 60, 60);

    hcf();
}