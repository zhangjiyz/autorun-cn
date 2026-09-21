/* Exercise the real backend with an audout ownership simulator. */
#include <assert.h>
#include <stdio.h>
#include "../source/audio_unix.c"

static unsigned int create_logs;
void wine_nx_runtime_trace(const char *msg)
{ assert(strstr(msg, "[NXAUDIO] create ")); create_logs++; }

static AudioOutBuffer *queued[NX_BUFFERS];
static unsigned int queued_count, ready;
static BOOL host_started;
Result audoutInitialize(void) { return 0; }
void audoutExit(void) { queued_count = ready = 0; host_started = FALSE; }
Result audoutStartAudioOut(void) { host_started = TRUE; return 0; }
Result audoutStopAudioOut(void) { host_started = FALSE; return 0; }
Result audoutAppendAudioOutBuffer(AudioOutBuffer *b)
{
    unsigned int i;
    assert(queued_count < NX_BUFFERS);
    assert(!((UINT_PTR)b->buffer & 0xfff));
    assert(b->data_size <= b->buffer_size);
    for (i = 0; i < queued_count; i++) assert(queued[i] != b);
    queued[queued_count++] = b;
    return 0;
}
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **b, u32 *count)
{
    *count = 0; *b = NULL;
    if (ready && queued_count)
    {
        unsigned int i;
        *b = queued[0]; *count = 1;
        for (i = 0; i + 1 < queued_count; i++) queued[i] = queued[i + 1];
        queued_count--; ready--;
    }
    return 0;
}
void armDCacheFlush(void *p, size_t size) { (void)p; (void)size; }
Result svcSetThreadPriority(Handle h, u32 priority) { (void)h; (void)priority; return 0; }
void *memalign(size_t alignment, size_t size)
{ void *p = NULL; if (posix_memalign(&p, alignment, size)) return NULL; return p; }
NTSTATUS WINAPI NtAllocateVirtualMemory(HANDLE proc, void **p, ULONG_PTR bits, SIZE_T *size, ULONG type, ULONG prot)
{
    (void)proc; (void)type; (void)prot;
    assert(bits == 0x7fffffff);
    *p = calloc(1, *size); return *p ? 0 : STATUS_NO_MEMORY;
}
NTSTATUS WINAPI NtFreeVirtualMemory(HANDLE proc, void **p, SIZE_T *size, ULONG type)
{ (void)proc; (void)size; (void)type; free(*p); *p = NULL; return 0; }
NTSTATUS WINAPI NtSetEvent(HANDLE event, LONG *state) { (void)event; (void)state; return 0; }
NTSTATUS WINAPI NtWaitForSingleObject(HANDLE h, BOOLEAN alert, const LARGE_INTEGER *time)
{ (void)h; (void)alert; (void)time; return 0; }
NTSTATUS WINAPI NtClose(HANDLE h) { (void)h; return 0; }
NTSTATUS WINAPI NtDelayExecution(BOOLEAN alert, const LARGE_INTEGER *time)
{ (void)alert; (void)time; return 0; }
NTSTATUS WINAPI NtQueryPerformanceCounter(LARGE_INTEGER *n, LARGE_INTEGER *f)
{ n->QuadPart = 10000000; if (f) f->QuadPart = 10000000; return 0; }

int main(void)
{
    struct test_connect_params connect = {0};
    WAVEFORMATEX fmt = {WAVE_FORMAT_PCM,2,48000,192000,4,16,0};
    stream_handle handle = 0;
    UINT32 channels = 0;
    struct create_stream_params create = {.flow=eRender, .share=AUDCLNT_SHAREMODE_SHARED,
        .duration=400000, .fmt=&fmt, .channel_count=&channels, .stream=&handle};
    const unsigned int total = (NX_BUFFERS + 1) * NX_CHUNK;
    BYTE *data = NULL;
    struct get_render_buffer_params get = {.frames=total, .data=&data};
    struct release_render_buffer_params put = {.written_frames=total};
    struct start_params startp;
    struct stop_params stopp;
    struct reset_params resetp;
    struct release_stream_params release;
    struct nx_audio_stream *s;
    unsigned int i, cycles, before;
    BOOL wrapped = FALSE;
    wine_nx_audio_unix_funcs[test_connect](&connect); assert(connect.priority == Priority_Preferred);
    wine_nx_audio_unix_funcs[create_stream](&create); assert(create.result == S_OK && channels == 2 && handle);
    assert(create_logs == 1);
    {
        stream_handle second_handle = 0;
        struct create_stream_params second = create;
        struct release_stream_params second_release;
        second.stream = &second_handle;
        nx_create_stream(&second);
        assert(second.result == S_OK && second_handle && create_logs == 2);
        second_release.stream = second_handle; second_release.timer_thread = NULL;
        nx_release_stream(&second_release);
        assert(second_release.result == S_OK && !streams[1]);
    }
    s = nx_stream(handle);
    get.stream = put.stream = handle;
    wine_nx_audio_unix_funcs[get_render_buffer](&get);
    assert(get.result == S_OK && data);
    for (i = 0; i < total; i++) { ((short *)data)[i*2] = i; ((short *)data)[i*2+1] = -i; }
    nx_release_render_buffer(&put); assert(put.result == S_OK && s->held == total);
    startp.stream = handle; nx_start(&startp);
    /* Only what audout can hold goes out; the rest stays owned by the ring. */
    assert(startp.result == S_OK && host_started && queued_count == NX_BUFFERS);
    assert(s->held == total && s->submitted == NX_BUFFERS * NX_CHUNK && !s->played);
    assert(((short *)queued[0]->buffer)[2] == 1);
    assert(((short *)queued[1]->buffer)[0] == NX_CHUNK);
    nx_pump(); assert(s->held == total && !s->played);
    /* A second shared client can run beside the first one.  The next hardware
     * buffer contains both clients instead of failing with DEVICE_IN_USE. */
    {
        stream_handle second_handle = 0;
        struct create_stream_params second = create;
        struct get_render_buffer_params second_get = {.frames=NX_CHUNK, .data=&data};
        struct release_render_buffer_params second_put = {.written_frames=NX_CHUNK};
        struct start_params second_start;
        struct release_stream_params second_release;
        struct nx_audio_stream *second_stream;
        second.stream = &second_handle;
        nx_create_stream(&second); assert(second.result == S_OK && second_handle);
        second_stream = nx_stream(second_handle);
        second_get.stream = second_put.stream = second_handle;
        nx_get_render_buffer(&second_get); assert(second_get.result == S_OK);
        for (i = 0; i < NX_CHUNK; i++) { ((short *)data)[i*2] = 100; ((short *)data)[i*2+1] = 200; }
        nx_release_render_buffer(&second_put); assert(second_put.result == S_OK);
        second_start.stream = second_handle; nx_start(&second_start); assert(second_start.result == S_OK);
        ready = 1; nx_pump();
        assert(second_stream->submitted == NX_CHUNK && queued_count == NX_BUFFERS);
        assert(((short *)queued[NX_BUFFERS - 1]->buffer)[0] == (short)(NX_BUFFERS * NX_CHUNK + 100));
        assert(((short *)queued[NX_BUFFERS - 1]->buffer)[1] == (short)(-(int)(NX_BUFFERS * NX_CHUNK) + 200));
        ready = NX_BUFFERS; nx_pump();
        assert(!second_stream->held && second_stream->played == NX_CHUNK);
        second_release.stream = second_handle; second_release.timer_thread = NULL;
        nx_release_stream(&second_release); assert(second_release.result == S_OK);
    }
    assert(!s->held && s->played == total && !queued_count);
    /* Cross the ring boundary, preserving order and silence. */
    put.flags = AUDCLNT_BUFFERFLAGS_SILENT;
    for (cycles = 0; cycles < 2 * s->capacity / NX_CHUNK; cycles++)
    {
        before = s->read;
        get.frames = put.written_frames = NX_CHUNK;
        nx_get_render_buffer(&get); assert(get.result == S_OK);
        nx_release_render_buffer(&put); assert(put.result == S_OK);
        nx_pump(); assert(queued_count == 1);
        for (i = 0; i < NX_CHUNK; i++) assert(((short *)queued[0]->buffer)[i*2] == 0);
        ready = 1; nx_pump();
        if (s->read < before) wrapped = TRUE;
    }
    assert(wrapped);
    resetp.stream = handle; nx_reset(&resetp); assert(resetp.result == AUDCLNT_E_NOT_STOPPED);
    stopp.stream = handle; nx_stop(&stopp); assert(stopp.result == S_OK && !host_started);
    nx_reset(&resetp); assert(resetp.result == S_OK && !s->held && !s->played && !queued_count);
    get.frames = s->capacity + 1; nx_get_render_buffer(&get); assert(get.result == AUDCLNT_E_BUFFER_TOO_LARGE);
    get.frames = 1; nx_get_render_buffer(&get); assert(get.result == S_OK);
    nx_get_render_buffer(&get); assert(get.result == AUDCLNT_E_OUT_OF_ORDER);
    put.written_frames = 2; nx_release_render_buffer(&put); assert(put.result == AUDCLNT_E_INVALID_SIZE);
    nx_reset(&resetp); assert(resetp.result == AUDCLNT_E_BUFFER_OPERATION_PENDING);
    put.written_frames = 0; nx_release_render_buffer(&put); assert(put.result == S_OK);
    release.stream = handle; release.timer_thread = NULL; nx_release_stream(&release);
    assert(release.result == S_OK && !streams[0] && !queued_count);
    /* Common application format: mono 44.1 kHz float is converted to the
     * Switch's stereo 48 kHz signed-16 stream. */
    {
        WAVEFORMATEX converted = {WAVE_FORMAT_IEEE_FLOAT, 1, 44100, 176400, 4, 32, 0};
        struct create_stream_params converted_create = {.flow=eRender, .share=AUDCLNT_SHAREMODE_SHARED,
            .duration=400000, .fmt=&converted, .channel_count=&channels, .stream=&handle};
        UINT32 source_capacity = 0, padding = 0;
        struct get_buffer_size_params sizep = {.frames=&source_capacity};
        struct get_current_padding_params paddingp = {.padding=&padding};
        float *source;
        nx_create_stream(&converted_create);
        assert(converted_create.result == S_OK && handle);
        sizep.stream = paddingp.stream = handle;
        nx_get_buffer_size(&sizep);
        assert(sizep.result == S_OK && source_capacity == nx_stream(handle)->source_capacity);
        assert(nx_stream(handle)->scratch_bytes == source_capacity * converted.nBlockAlign);
        get.stream = put.stream = handle; get.frames = put.written_frames = 441;
        nx_get_render_buffer(&get); assert(get.result == S_OK);
        source = (float *)data;
        for (i = 0; i < 441; i++) source[i] = i == 0 ? 0.5f : 0.0f;
        nx_release_render_buffer(&put); assert(put.result == S_OK && nx_stream(handle)->held == 480);
        nx_get_current_padding(&paddingp);
        assert(paddingp.result == S_OK && padding == 441);
        release.stream = handle; release.timer_thread = NULL; nx_release_stream(&release);
        assert(release.result == S_OK);
    }
    /* DirectSound (DSOUND_WaveFormat, DSOUND_ReopenDevice): the mix format made
     * 32-bit float EXTENSIBLE, everything else copied, then Initialize with event
     * callbacks. A plain WAVEFORMATEX mix format left cbSize 0 there, which
     * mmdevapi's validate_wfx rejects with E_INVALIDARG. */
    {
        WAVEFORMATEXTENSIBLE mix, ds;
        struct get_mix_format_params mixp = {.flow=eRender, .fmt=&mix};
        struct is_format_supported_params supported = {.flow=eRender, .share=AUDCLNT_SHAREMODE_SHARED, .fmt_in=&ds.Format};
        struct create_stream_params ds_create = {.flow=eRender, .share=AUDCLNT_SHAREMODE_SHARED,
            .flags=AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
            .duration=800000, .fmt=&ds.Format, .channel_count=&channels, .stream=&handle};
        float *source;
        nx_get_mix_format(&mixp);
        assert(mixp.result == S_OK && mix.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
        assert(mix.Format.nChannels == 2 && mix.dwChannelMask == 0x3);
        ds = mix;
        ds.SubFormat = nx_subtype_float;
        ds.Samples.wValidBitsPerSample = ds.Format.wBitsPerSample = 32;
        ds.Format.nBlockAlign = ds.Format.nChannels * ds.Format.wBitsPerSample / 8;
        ds.Format.nAvgBytesPerSec = ds.Format.nSamplesPerSec * ds.Format.nBlockAlign;
        assert(ds.Format.cbSize == sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
        nx_is_format_supported(&supported); assert(supported.result == S_OK);
        nx_create_stream(&ds_create); assert(ds_create.result == S_OK && handle);
        assert(nx_stream(handle)->source_tag == WAVE_FORMAT_IEEE_FLOAT);
        get.stream = put.stream = handle; get.frames = put.written_frames = 480;
        put.flags = 0;  /* the ring-wrap loop above left AUDCLNT_BUFFERFLAGS_SILENT set */
        nx_get_render_buffer(&get); assert(get.result == S_OK);
        source = (float *)data;
        for (i = 0; i < 960; i++) source[i] = i == 0 ? 0.5f : 0.0f;
        nx_release_render_buffer(&put); assert(put.result == S_OK && nx_stream(handle)->held == 480);
        /* float, not read as 32-bit PCM */
        assert(((short *)nx_stream(handle)->ring)[0] == 16383 && ((short *)nx_stream(handle)->ring)[1] == 0);
        release.stream = handle; release.timer_thread = NULL; nx_release_stream(&release);
        assert(release.result == S_OK);
    }
    {
        BYTE endpoint_buffer[256];
        struct get_endpoint_ids_params endpoints =
        {
            .flow = eRender, .endpoints = (struct endpoint *)endpoint_buffer,
            .size = sizeof(endpoint_buffer)
        };
        WCHAR driver[8] = {0xcccc};
        UINT err = 0xcccc;
        BOOL quit = FALSE;
        struct notify_context notify = {.send_notify = TRUE};
        struct midi_init_params midi_init_params = {.err = &err};
        struct midi_out_message_params midi_out = {.msg = MODM_GETNUMDEVS, .err = &err, .notify = &notify};
        struct midi_in_message_params midi_in = {.msg = MIDM_OPEN, .err = &err, .notify = &notify};
        struct midi_notify_wait_params midi_wait = {.quit = &quit, .notify = &notify};
        struct aux_message_params aux = {.msg = AUXDM_GETNUMDEVS, .err = &err};

        assert((UINT_PTR)&endpoints > 0xffffffffu && (UINT_PTR)endpoint_buffer > 0xffffffffu);
        wine_nx_audio_unix_funcs[get_endpoint_ids](&endpoints);
        assert(endpoints.result == S_OK && endpoints.num == 1 && endpoints.size <= sizeof(endpoint_buffer));
        wine_nx_audio_unix_funcs[midi_get_driver](driver);
        assert(!driver[0]);
        wine_nx_audio_unix_funcs[midi_init](&midi_init_params);
        assert(!err);
        wine_nx_audio_unix_funcs[midi_out_message](&midi_out);
        assert(!err && !notify.send_notify);
        notify.send_notify = TRUE;
        wine_nx_audio_unix_funcs[midi_in_message](&midi_in);
        assert(err == MMSYSERR_BADDEVICEID && !notify.send_notify);
        notify.send_notify = TRUE;
        wine_nx_audio_unix_funcs[midi_notify_wait](&midi_wait);
        assert(quit && !notify.send_notify);
        wine_nx_audio_unix_funcs[aux_message](&aux);
        assert(!err);
        aux.msg = AUXDM_GETDEVCAPS;
        wine_nx_audio_unix_funcs[aux_message](&aux);
        assert(err == MMSYSERR_BADDEVICEID);
    }
    puts("Audio backend: native table, 64-bit pointers, zero MIDI/aux devices, playback and formats passed");
    return 0;
}
