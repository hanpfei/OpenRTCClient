package com.example.audioplayout;

import android.Manifest;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Bundle;

import com.google.android.material.snackbar.Snackbar;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.view.View;

import androidx.core.app.ActivityCompat;
import androidx.navigation.ui.AppBarConfiguration;

import com.example.audioplayout.databinding.ActivityMainBinding;

import android.view.Menu;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity
        implements ActivityCompat.OnRequestPermissionsResultCallback {
    private static final String TAG = "MainActivity";
    private static final int AUDIO_PLAYOUT_REQUEST = 0;

    private ActivityMainBinding binding;
    private Button button;
    private boolean playing;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ContextUtils.initialize(getApplicationContext());
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setSupportActionBar(binding.toolbar);

        button = (Button)findViewById((R.id.button_first));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (ActivityCompat.checkSelfPermission(MainActivity.this, Manifest.permission.WRITE_EXTERNAL_STORAGE) !=
                        PackageManager.PERMISSION_GRANTED) {
                    ActivityCompat.requestPermissions(
                            MainActivity.this,
                            new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE },
                            AUDIO_PLAYOUT_REQUEST);
                    return;
                }
                startPlayout();
            }
        });
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        /*
         * if any permission failed, the sample could not play
         */
        if (AUDIO_PLAYOUT_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1  ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            /*
             * When user denied permission, throw a Toast to prompt that RECORD_AUDIO
             * is necessary; also display the status on UI
             * Then application goes back to the original state: it behaves as if the button
             * was not clicked. The assumption is that user will re-click the "start" button
             * (to retry), or shutdown the app in normal way.
             */
            Toast.makeText(getApplicationContext(),
                    "External storage access permission",
                    Toast.LENGTH_SHORT).show();
            return;
        }
        startPlayout();
    }

    private void startPlayout() {
        if (!playing) {
            AudioEngine.CreateAudioEngine(AudioDefines.AudioLayer.kAndroidJavaAudio);
            AudioEngine.CreateAudioPlayer(AudioManager.STREAM_VOICE_CALL);
            AudioEngine.StartPlayFile("/sdcard/new_morning_48k_2ch_16bits.wav");

            button.setText(R.string.stop_play);
            playing = true;
        } else {
            AudioEngine.StopPlay();
            AudioEngine.DeleteAudioPlayer();
            AudioEngine.DeleteAudioEngine();

            button.setText(R.string.start_play);
            playing = false;
        }
    }
}