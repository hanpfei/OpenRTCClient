
#include <stdio.h>

#include "lame.h"

#include "pulseaudio_player.h"

#define INBUF_SIZE (4096)
#define BUF_SIZE (512)
#define MP3_DATA_BUF_SIZE ((int)(1.25 * BUF_SIZE) + 7200)
#define PCM_DATA_BUF_SIZE (sizeof(int16_t) * INBUF_SIZE * 2)

static int interleave(int16_t *pcm_data, int16_t *pcm_l, int16_t *pcm_r,
                      int samples) {
  for (int i = 0; i < samples; ++i) {
    pcm_data[2 * i] = pcm_l[i];
    pcm_data[2 * i + 1] = pcm_r[i];
  }
  return 0;
}

int main(int argc, char *argv[]) {
  const char *in_mp3_file_path;
  FILE *fp_in_mp3 = NULL;

  uint8_t mp3_data_buf[MP3_DATA_BUF_SIZE] = {0};
  int read_mp3_bytes = -1;

  hip_t hip = NULL;

  int sample_rate = -1;
  int channels = -1;

  short pcm_l[INBUF_SIZE] = {0};
  short pcm_r[INBUF_SIZE] = {0};
  int samples = -1;

  uint8_t pcm_data_buf[PCM_DATA_BUF_SIZE] = {0};

  if (argc < 2) {
    printf("Usage: %s [mp3_file_path]\n", argv[0]);
    return -1;
  }

  in_mp3_file_path = argv[1];
  printf("MP3 file path: %s\n", in_mp3_file_path);

  fp_in_mp3 = fopen(in_mp3_file_path, "rb");
  if (!fp_in_mp3) {
    perror("open input MP3 file failed");
    return -1;
  }

  hip = hip_decode_init();
  if (!hip) {
    printf("init mp3 decoder failed!\n");
    fclose(fp_in_mp3);
    return -1;
  }

  mp3data_struct mp3_info = {};

  /* MP3 decode 2/4: read MP3 header info */
  do {
    read_mp3_bytes = fread(mp3_data_buf, 1, 16, fp_in_mp3);
    samples = hip_decode_headers(hip, mp3_data_buf, read_mp3_bytes, pcm_l,
                                 pcm_r, &mp3_info);

    sample_rate = mp3_info.samplerate;
    channels = mp3_info.stereo;
  } while (!mp3_info.header_parsed && read_mp3_bytes > 0);

  fprintf(stderr, "MP3 sample rate %d, channels %d, samples %d.\n", sample_rate,
          channels, samples);

  auto player = PulseAudioPlayer::create(channels, sample_rate);
  if (!player) {
    fprintf(stderr, "Create player failed.\n");
    if (hip) {
      hip_decode_exit(hip);
    }

    if (fp_in_mp3) {
      fclose(fp_in_mp3);
    }
    return -1;
  }

  if (samples > 0) {
    if (channels == 1) {
      player->writeData(pcm_l, sizeof(int16_t) * samples);
    } else if (channels == 2) {
      interleave(reinterpret_cast<int16_t *>(pcm_data_buf),
                 reinterpret_cast<int16_t *>(pcm_l),
                 reinterpret_cast<int16_t *>(pcm_r), samples);
      player->writeData(pcm_data_buf, sizeof(int16_t) * 2 * samples);
    }
  }

  while (1) {
    read_mp3_bytes = fread(mp3_data_buf, 1, 418, fp_in_mp3);
    if (read_mp3_bytes <= 0)
      break;

    /* MP3 decode 3/4: decode MP3 data */
    samples = hip_decode(hip, mp3_data_buf, read_mp3_bytes, pcm_l, pcm_r);
    if (samples > 0) {
//      printf("read MP3 bytes: %d \t decode output samples: %d\n",
//             read_mp3_bytes, samples);
      if (channels == 1) {
        player->writeData(pcm_l, sizeof(int16_t) * samples);
      } else if (channels == 2) {
        interleave(reinterpret_cast<int16_t *>(pcm_data_buf),
                   reinterpret_cast<int16_t *>(pcm_l),
                   reinterpret_cast<int16_t *>(pcm_r), samples);
        player->writeData(pcm_data_buf, sizeof(int16_t) * 2 * samples);
      }
    }
  }

  if (hip) {
    hip_decode_exit(hip);
  }

  if (fp_in_mp3) {
    fclose(fp_in_mp3);
  }

  return 0;
}
