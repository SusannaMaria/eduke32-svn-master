// Build engine timer stuff

#include "timer.h"

#include "build.h"
#include "compat.h"
#include "build_cpuid.h"
#include "renderlayer.h"

#ifdef _WIN32
#include "winbits.h"
#include <mmsystem.h>
#endif

#include <chrono>
#include <atomic>

#if (defined RENDERTYPESDL) && (SDL_MAJOR_VERSION >= 2)
#define HAVE_TIMER_SDL
#endif

using namespace std;
using namespace chrono;

static int sys_timer;

EDUKE32_STATIC_ASSERT(steady_clock::is_steady);

static time_point<steady_clock> clockLastSampleTime;
static int clockTicksPerSecond;
static void(*usertimercallback)(void) = nullptr;

#ifdef ZPL_HAVE_RDTSC
static uint64_t tsc_freq;

static FORCE_INLINE uint64_t timerSampleRDTSC()
{
    // We need to serialize the instruction stream before executing RDTSC.
#if defined __SSE2__
    // On AMD, MFENCE serializes and LFENCE does not. On Intel, LFENCE serializes and MFENCE does not.
    // https://stackoverflow.com/a/50332912
    // MFENCE before LFENCE is preferable since we're using both.
    // https://www.felixcloutier.com/x86/rdtsc
    // https://hadibrais.wordpress.com/2018/05/14/the-significance-of-the-x86-lfence-instruction/
    _mm_mfence();
    _mm_lfence();

    // Fallbacks on x86 without SSE2 use a LOCK prefix.
    // The alternative is the CPUID instruction, but benchmarking would be desirable to pick the best choice.
#elif defined __GNUC__
    __sync_synchronize();
#else
    atomic_thread_fence(memory_order_seq_cst);
#endif

    uint64_t const result = zpl_rdtsc();

    // Some sources suggest serialization is also necessary or desirable after RDTSC. For now, don't.

    return result;
}
#endif

int timerGetClockRate(void) { return clockTicksPerSecond; }

ATTRIBUTE((flatten)) void timerUpdateClock(void)
{
    auto time = steady_clock::now();
    auto elapsedTime = time - clockLastSampleTime;

    uint64_t numerator = (elapsedTime.count() * (uint64_t) clockTicksPerSecond * steady_clock::period::num);
    uint64_t const freq = steady_clock::period::den;
    int n = tabledivide64(numerator, freq);
    totalclock.setFraction(tabledivide64((numerator - n*freq) * 65536, freq));

    if (n <= 0) return;

    totalclock += n;
    clockLastSampleTime += n*nanoseconds(1000000000/clockTicksPerSecond);

    if (usertimercallback)
        for (; n > 0; n--) usertimercallback();
}

uint32_t timerGetTicks(void)
{
#ifdef _WIN32
    return timeGetTime();
#else
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

// Returns the time since an unspecified starting time in milliseconds (fractional).
// (May be not monotonic for certain configurations.)
double timerGetHiTicks(void) { return duration<double, nano>(steady_clock::now().time_since_epoch()).count() / 1000000.0; }

uint64_t timerGetTicksU64(void)
{
    switch (sys_timer)
    {
#ifdef HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
        case TIMER_SDL:
            return SDL_GetPerformanceCounter();
#elif !defined _WIN32 && !defined HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
#endif // HAVE_TIMER_SDL
#ifdef _WIN32
#if !defined HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
#endif // !HAVE_TIMER_SDL
        case TIMER_QPC:
            LARGE_INTEGER li;
            QueryPerformanceCounter(&li);
            return li.QuadPart;
#endif // _WIN32
#ifdef ZPL_HAVE_RDTSC
        case TIMER_RDTSC:
            return timerSampleRDTSC();
#endif
    }
}

uint64_t timerGetFreqU64(void)
{
    switch (sys_timer)
    {
#ifdef HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
        case TIMER_SDL:
        {
            static uint64_t freq;
            if (freq == 0)
                freq = SDL_GetPerformanceFrequency();
            return freq;
        }
#elif !defined _WIN32 && !defined HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
#endif // HAVE_TIMER_SDL
#ifdef _WIN32
#if !defined HAVE_TIMER_SDL
        default:
        case TIMER_AUTO:
#endif // !HAVE_TIMER_SDL
        case TIMER_QPC:
        {
            static LARGE_INTEGER li;
            if (li.QuadPart == 0)
                QueryPerformanceFrequency(&li);
            return li.QuadPart;
        }
#endif // _WIN32
#ifdef ZPL_HAVE_RDTSC
        case TIMER_RDTSC:
            return tsc_freq;
#endif
    }
}

static int osdcmd_sys_timer(osdcmdptr_t parm)
{
    static char constexpr const *s[] = { "auto", "QueryPerformanceCounter", "SDL", "RDTSC instruction" };
    int const r = osdcmd_cvar_set(parm);

    if (r != OSDCMD_OK)
        goto print_and_return;

#ifndef _WIN32
    if (sys_timer == TIMER_QPC)
        sys_timer = TIMER_AUTO;
#endif
#ifndef HAVE_TIMER_SDL
    if (sys_timer == TIMER_SDL)
        sys_timer = TIMER_AUTO;
#endif

#if defined EDUKE32_CPU_X86 && defined ZPL_HAVE_RDTSC
    if (sys_timer == TIMER_RDTSC && !cpu.features.invariant_tsc)
    {
        sys_timer = TIMER_AUTO;
        OSD_Printf("Invariant TSC support not detected.\n");
    }
#endif

    if (sys_timer != TIMER_AUTO || !OSD_ParsingScript())
print_and_return:
        OSD_Printf("Using \"%s\" timer with %g MHz frequency\n", s[sys_timer], timerGetFreqU64() / 1000000.0);

    return r;
}

int timerInit(int const tickspersecond)
{
    static int initDone;

    if (initDone == 0)
    {
        static osdcvardata_t sys_timer_cvar = { "sys_timer",
                                                "engine frame timing backend:\n"
                                                "   0: auto\n"
#ifdef _WIN32
                                                "   1: QueryPerformanceCounter\n"
#endif
#ifdef HAVE_TIMER_SDL
                                                "   2: SDL timer\n"
#endif
#ifdef ZPL_HAVE_RDTSC
                                                "   3: RDTSC instruction\n"
#endif
                                                , (void *)&sys_timer, CVAR_INT | CVAR_FUNCPTR, 0, 4 };

        OSD_RegisterCvar(&sys_timer_cvar, osdcmd_sys_timer);

#ifdef HAVE_TIMER_SDL
        SDL_InitSubSystem(SDL_INIT_TIMER);
#endif

#ifdef ZPL_HAVE_RDTSC
        if (tsc_freq == 0)
        {
            double const calibrationEndTime = timerGetHiTicks() + 100.0;
            auto const time1 = timerSampleRDTSC();
            do { } while (timerGetHiTicks() < calibrationEndTime);
            auto const time2 = timerSampleRDTSC();
            auto const time3 = timerSampleRDTSC() - time2;

            tsc_freq = (time2 - time1 - time3) * 10;
        }
#endif
        initDone = 1;
    }

    clockTicksPerSecond = tickspersecond;
    clockLastSampleTime = steady_clock::now();

    usertimercallback = nullptr;

    return 0;
}

void(*timerSetCallback(void(*callback)(void)))(void)
{
    void(*oldtimercallback)(void);

    oldtimercallback = usertimercallback;
    usertimercallback = callback;

    return oldtimercallback;
}
