struct pcm;

extern int pcm_state(struct pcm *pcm);

#ifndef PCM_STATE_RUNNING
#define PCM_STATE_RUNNING 0x03
#endif

__attribute__((visibility("default")))
int pcm_out_is_running(struct pcm *pcm) {
    return pcm && pcm_state(pcm) == PCM_STATE_RUNNING;
}
