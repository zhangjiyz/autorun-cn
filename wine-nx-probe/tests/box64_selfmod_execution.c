/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
/* Unreported guest code writes: opt-in hash validation, with default/interpreter controls. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "wow64_box64_engine.h"
#define BASE 0x10000000u
#define SIZE 0x20000u
extern char wine_nx_box64_options_path[512];
static NTSTATUS read_guest(void *opaque, ULONG address, void *buffer, SIZE_T size)
{
    (void)opaque;
    if(address < BASE || address - BASE >= SIZE || size > SIZE - (address - BASE)) return STATUS_ACCESS_VIOLATION;
    memcpy(buffer,(void *)(uintptr_t)address,size); return 0;
}
static void init_context(I386_CONTEXT *ctx)
{
    XMM_SAVE_AREA32 fx={0};
    memset(ctx,0,sizeof(*ctx)); ctx->ContextFlags=CONTEXT_I386_ALL;
    ctx->Eip=BASE+0x200;ctx->Esp=BASE+0x6000;ctx->EFlags=0x202;
    ctx->SegCs=0x23;ctx->SegSs=ctx->SegDs=ctx->SegEs=0x2b;ctx->SegFs=0x53;
    ctx->FloatSave.ControlWord=0x37f;ctx->FloatSave.TagWord=0xffff;
    fx.ControlWord=0x37f;fx.MxCsr=0x1f80;memcpy(ctx->ExtendedRegisters,&fx,sizeof(fx));
}
int main(int argc,char **argv)
{
    unsigned char *memory=mmap((void *)(uintptr_t)BASE,SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    I386_CONTEXT ctx;ULONGLONG executed;
    const struct wine_nx_wow64_host host={read_guest,NULL,NULL,NULL};
    const struct wine_nx_wow64_gates gates={BASE+0x8000,BASE+0x8010};
    static const unsigned char code[]={0xb8,17,0,0,0,0xba,0x20,0x80,0,0x10,0xff,0xe2};
    assert(memory != MAP_FAILED && (argc==3 || argc==4));
    if(argc==3 || strcmp(argv[3],"unregistered"))
        wine_nx_box64_set_main_image(BASE + (argc==4 ? 0x1000 : 0), 0x1000);
    snprintf(wine_nx_box64_options_path,sizeof(wine_nx_box64_options_path),"%s",argv[1]);
    memcpy(memory+0x200,code,sizeof(code));
    init_context(&ctx);assert(!wine_nx_box64_run(&ctx,0,&gates,&host,NULL,BASE+0x8020,1000,&executed));
    assert(ctx.Eax==17);
    /* The guest rewrites its code without FlushInstructionCache or protection changes. */
    memory[0x201]=29;
    init_context(&ctx);assert(!wine_nx_box64_run(&ctx,0,&gates,&host,NULL,BASE+0x8020,1000,&executed));
    printf("rewritten immediate=29, executed result=%u\n",(unsigned)ctx.Eax);
    assert(ctx.Eax==(unsigned)atoi(argv[2]));
    return 0;
}
