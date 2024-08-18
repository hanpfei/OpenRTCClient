
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_wav_file_writer.h"
#include "pa_recorder.h"

#include "lame.h"

static bool stopped = false;

void sig_handler(int sig) {
  printf("Caught signal %d\n", sig);

  stopped = true;
}

void encoder_progress_begin(lame_global_flags const *gf, char const *outPath) {
  lame_print_config(gf); /* print useful information about options being used */

  printf("Encoding to %s\n", outPath);
  printf("Encoding as %g kHz ", 1.e-3 * lame_get_out_samplerate(gf));

  {
    static const char *mode_names[2][4] = {
        {"stereo", "j-stereo", "dual-ch", "single-ch"},
        {"stereo", "force-ms", "dual-ch", "single-ch"}};
    switch (lame_get_VBR(gf)) {
    case vbr_rh:
      printf("%s MPEG-%u%s Layer III VBR(q=%g) qval=%i\n",
             mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
             2 - lame_get_version(gf),
             lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
             lame_get_VBR_quality(gf), lame_get_quality(gf));
      break;
    case vbr_mt:
    case vbr_mtrh:
      printf("%s MPEG-%u%s Layer III VBR(q=%g)\n",
             mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
             2 - lame_get_version(gf),
             lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
             lame_get_VBR_quality(gf));
      break;
    case vbr_abr:
      printf("%s MPEG-%u%s Layer III (%gx) average %d kbps qval=%i\n",
             mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
             2 - lame_get_version(gf),
             lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
             0.1 * (int)(10. * lame_get_compression_ratio(gf) + 0.5),
             lame_get_VBR_mean_bitrate_kbps(gf), lame_get_quality(gf));
      break;
    default:
      printf("%s MPEG-%u%s Layer III (%gx) %3d kbps qval=%i\n",
             mode_names[lame_get_force_ms(gf)][lame_get_mode(gf)],
             2 - lame_get_version(gf),
             lame_get_out_samplerate(gf) < 16000 ? ".5" : "",
             0.1 * (int)(10. * lame_get_compression_ratio(gf) + 0.5),
             lame_get_brate(gf), lame_get_quality(gf));
      break;
    }
  }
}

static int write_id3v1_tag(lame_t gf, FILE *outf) {
  unsigned char mp3buffer[128];
  size_t imp3, owrite;

  imp3 = lame_get_id3v1_tag(gf, mp3buffer, sizeof(mp3buffer));
  if (imp3 <= 0) {
    return 0;
  }
  if (imp3 > sizeof(mp3buffer)) {
    fprintf(stderr,
            "Error writing ID3v1 tag: buffer too small: buffer size=%lu  ID3v1 "
            "size=%zu\n",
            sizeof(mp3buffer), imp3);
    return 0; /* not critical */
  }
  owrite = fwrite(mp3buffer, 1, imp3, outf);
  if (owrite != imp3) {
    fprintf(stderr, "Error writing ID3v1 tag \n");
    return 1;
  }
  return 0;
}

static int write_xing_frame(lame_global_flags *gf, FILE *outf, size_t offset) {
  unsigned char mp3buffer[LAME_MAXMP3BUFFER];
  size_t imp3, owrite;

  imp3 = lame_get_lametag_frame(gf, mp3buffer, sizeof(mp3buffer));
  if (imp3 <= 0) {
    return 0; /* nothing to do */
  }

  printf("Writing LAME Tag...");

  if (imp3 > sizeof(mp3buffer)) {
    fprintf(stderr,
            "Error writing LAME-tag frame: buffer too small: buffer size=%lu  "
            "frame size=%zu\n",
            sizeof(mp3buffer), imp3);
    return -1;
  }
  assert(offset <= LONG_MAX);
  if (fseek(outf, (long)offset, SEEK_SET) != 0) {
    fprintf(stderr, "fatal error: can't update LAME-tag frame!\n");
    return -1;
  }
  owrite = fwrite(mp3buffer, 1, imp3, outf);
  if (owrite != imp3) {
    fprintf(stderr, "Error writing LAME-tag \n");
    return -1;
  }

  printf("done\n");

  assert(imp3 <= INT_MAX);
  return (int)imp3;
}

static void print_trailing_info(lame_global_flags *gf) {
  if (lame_get_findReplayGain(gf)) {
    int RadioGain = lame_get_RadioGain(gf);
    printf("ReplayGain: %s%.1fdB\n", RadioGain > 0 ? "+" : "",
           ((float)RadioGain) / 10.0);
    if (RadioGain > 0x1FE || RadioGain < -0x1FE)
      printf("WARNING: ReplayGain exceeds the -51dB to +51dB range. Such a "
             "result is too\n"
             "         high to be stored in the header.\n");
  }

  /* if (the user requested printing info about clipping) and (decoding
     on the fly has actually been performed) */
  if (lame_get_decode_on_the_fly(gf)) {
    float noclipGainChange = (float)lame_get_noclipGainChange(gf) / 10.0f;
    float noclipScale = lame_get_noclipScale(gf);

    if (noclipGainChange > 0.0) { /* clipping occurs */
      printf("WARNING: clipping occurs at the current gain. Set your decoder "
             "to decrease\n"
             "         the  gain  by  at least %.1fdB or encode again ",
             noclipGainChange);

      /* advice the user on the scale factor */
      if (noclipScale > 0) {
        printf("using  --scale %.2f\n", noclipScale * lame_get_scale(gf));
        printf("         or less (the value under --scale is approximate).\n");
      } else {
        /* the user specified his own scale factor. We could suggest
         * the scale factor of (32767.0/gfp->PeakSample)*(gfp->scale)
         * but it's usually very inaccurate. So we'd rather advice him to
         * disable scaling first and see our suggestion on the scale factor
         * then. */
        printf("using --scale <arg>\n"
               "         (For   a   suggestion  on  the  optimal  value  of  "
               "<arg>  encode\n"
               "         with  --scale 1  first)\n");
      }

    } else { /* no clipping */
      if (noclipGainChange > -0.1)
        printf("\nThe waveform does not clip and is less than 0.1dB away from "
               "full scale.\n");
      else
        printf("\nThe waveform does not clip and is at least %.1fdB away from "
               "full scale.\n",
               -noclipGainChange);
    }
  }
}

static int
lame_encoder_loop(lame_global_flags *gf, FILE *outf, const char *outPath,
                  std::shared_ptr<PulseAudioRecorder> recorder,
                  std::shared_ptr<AudioWavFileWriter> wav_file_writer) {
  unsigned char mp3buffer[LAME_MAXMP3BUFFER];
  short int buffer[2 * 1152];
  int imp3, owrite, in_limit = 0;
  size_t id3v2_size;

  encoder_progress_begin(gf, outPath);

  id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
  if (id3v2_size > 0) {
    unsigned char *id3v2tag =
        reinterpret_cast<unsigned char *>(malloc(id3v2_size));
    if (id3v2tag != 0) {
      size_t n_bytes = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
      size_t written = fwrite(id3v2tag, 1, n_bytes, outf);
      free(id3v2tag);
      if (written != n_bytes) {
        fprintf(stderr, "Error writing ID3v2 tag \n");
        return 1;
      }
    }
  }

  /* do not feed more than in_limit PCM samples in one encode call
     otherwise the mp3buffer is likely too small
   */
  in_limit = lame_get_maximum_number_of_samples(gf, sizeof(mp3buffer));
  if (in_limit < 1) {
    in_limit = 1;
  }

  AudioPcmDataInfo audioFrameInfo;
  audioFrameInfo.samplesOut = sizeof(buffer) / sizeof(int16_t);

  /* encode until we hit stop */
  do {
    recorder->readData(buffer, sizeof(buffer));
    wav_file_writer->writeWavAudioPcmData(buffer, audioFrameInfo);

    int rest = sizeof(buffer);
    do {
      int const chunk = rest < in_limit ? rest : in_limit;

      /* encode */
      imp3 = lame_encode_buffer_interleaved(gf, buffer, 1152, mp3buffer,
                                            sizeof(mp3buffer));
      rest -= chunk;
      /* was our output buffer big enough? */
      if (imp3 < 0) {
        if (imp3 == -1)
          fprintf(stderr, "mp3 buffer is not big enough... \n");
        else
          fprintf(stderr, "mp3 internal error:  error code=%i\n", imp3);
        return 1;
      }
      owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
      if (owrite != imp3) {
        fprintf(stderr, "Error writing mp3 output \n");
        return 1;
      }
    } while (rest > 0);
    fflush(outf);
  } while (!stopped);

  imp3 = lame_encode_flush(
      gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */

  if (imp3 < 0) {
    if (imp3 == -1)
      fprintf(stderr, "mp3 buffer is not big enough... \n");
    else
      fprintf(stderr, "mp3 internal error:  error code=%i\n", imp3);
    return 1;
  }

  owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
  if (owrite != imp3) {
    fprintf(stderr, "Error writing mp3 output \n");
    return 1;
  }
  fflush(outf);
  imp3 = write_id3v1_tag(gf, outf);
  fflush(outf);
  if (imp3) {
    return 1;
  }
  write_xing_frame(gf, outf, id3v2_size);
  fflush(outf);
  print_trailing_info(gf);
  return 0;
}

int main(int argc, char *argv[]) {
  const char *out_mp3_file_path;
  FILE *fp_out_mp3 = NULL;

  size_t nChannels = 2;
  uint32_t sampleRateHz = 48000;
  lame_global_flags *gfp;

  if (argc < 2) {
    printf("Usage: %s [mp3_file_path]\n", argv[0]);
    return -1;
  }

  out_mp3_file_path = argv[1];
  printf("MP3 file path: %s\n", out_mp3_file_path);

  fp_out_mp3 = fopen(out_mp3_file_path, "wb");
  if (!fp_out_mp3) {
    perror("open input MP3 file failed");
    return -1;
  }

  signal(SIGINT, sig_handler);

  std::shared_ptr<AudioWavFileWriter> wav_file_writer =
      AudioWavFileWriter::CreateFileWrite("dump.wav", nChannels, sampleRateHz);

  std::shared_ptr<PulseAudioRecorder> recorder =
      PulseAudioRecorder::create(nChannels, sampleRateHz);
  if (!recorder) {
    fprintf(stderr, "Create player failed.\n");
    return -1;
  }

  gfp = lame_init();
  lame_set_in_samplerate(gfp, sampleRateHz);
  lame_set_out_samplerate(gfp, sampleRateHz);
  lame_set_num_channels(gfp, nChannels);
  lame_set_mode(gfp, STEREO);
  lame_set_VBR(gfp, vbr_mtrh);

  //  lame_set_brate();
  //  lame_set_VBR_mean_bitrate_kbps();
  lame_init_params(gfp);

  lame_encoder_loop(gfp, fp_out_mp3, out_mp3_file_path, recorder,
                    wav_file_writer);

  wav_file_writer->postWriteWavFile();

  lame_close(gfp);

  if (fp_out_mp3) {
    fclose(fp_out_mp3);
  }

  return 0;
}
