package com.example.jnitest;

import android.os.Bundle;

import com.google.android.material.snackbar.Snackbar;

import androidx.appcompat.app.AppCompatActivity;

import android.util.Log;
import android.view.View;

import androidx.navigation.ui.AppBarConfiguration;

import com.example.jnitest.databinding.ActivityMainBinding;

import android.view.Menu;
import android.view.MenuItem;
import android.widget.Button;

import java.util.Arrays;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";

    private AppBarConfiguration appBarConfiguration;
    private ActivityMainBinding binding;
    private Button button;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setSupportActionBar(binding.toolbar);

        binding.fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                        .setAction("Action", null).show();
            }
        });

        button = (Button)findViewById((R.id.button_first));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Log.w(TAG, "booleanParam false, returned " + JniUtils.booleanParam(false));
                Log.w(TAG, "byteParam 32, returned " + JniUtils.byteParam((byte) 32));
                Log.w(TAG, "charParam 'A', returned " + JniUtils.charParam('A'));
                Log.w(TAG, "shortParam 64, returned " + JniUtils.shortParam((short) 64));
                Log.w(TAG, "intParam 400, returned " + JniUtils.intParam(400));
                Log.w(TAG, "longParam 5000, returned " + JniUtils.longParam(5000));
                Log.w(TAG, "floatParam 142.214f, returned " + JniUtils.floatParam(142.214f));
                Log.w(TAG, "doubleParam 345435.253, returned " + JniUtils.doubleParam(345435.253));
                Log.w(TAG, "voidParam returned " + JniUtils.voidParam());

                JniUtils utils = new JniUtils();
                Log.w(TAG, "intArrayParam [1, 2, 3, 4, 5] returned " + Arrays.toString(utils.intArrayParam(new int[] {1, 2, 3, 4, 5})));
                Log.w(TAG, "stringParam 'String from java' returned '" + utils.stringParam("String from java") + "'.");

                try {
                    utils.methodThrowException("Message");
                } catch (RuntimeException e) {
                    e.printStackTrace();
                }

                utils.startNativeThread(1234);
                try {
                    Thread.sleep(5);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                utils.stopNativeThread();
            }
        });
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }
}