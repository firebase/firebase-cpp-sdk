package com.google.firebase.gameloop

import android.content.Intent
import android.content.pm.ResolveInfo
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import java.io.File


class MainActivity : AppCompatActivity(R.layout.activity_main) {
  companion object {
    const val GAMELOOP_INTENT = "com.google.intent.action.TEST_LOOP"
    const val TEST_LOOP_REQUEST_CODE = 1
  }

  private lateinit var testingTV: TextView
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    testingTV = findViewById(R.id.test)
    launchGame()
  }

  private fun launchGame() {
    val intent = Intent(GAMELOOP_INTENT, null)
    intent.addCategory(Intent.CATEGORY_DEFAULT)
    intent.type = "application/javascript"
    val pkgAppsList: List<ResolveInfo> = packageManager.queryIntentActivities(intent, 0)
    val gamePackageName = pkgAppsList[0].activityInfo.packageName

    val dir = File(getFilesDir(), gamePackageName)
    if (!dir.exists()) dir.mkdirs()
    val filename = "Results1.json"
    val file = File(dir, filename)
    file.createNewFile()
    Log.d("TAG", "Test Result Path :" + file)
    val fileUri: Uri = FileProvider.getUriForFile(this, "com.google.firebase.gameloop.fileprovider", file)

    intent.setPackage(gamePackageName)
      .setDataAndType(fileUri, "application/javascript").flags = Intent.FLAG_GRANT_WRITE_URI_PERMISSION

    if (android.os.Build.FINGERPRINT.contains("generic")) {
    	intent.putExtra("USE_FIRESTORE_EMULATOR", "true")
    }

    startActivityForResult(intent, TEST_LOOP_REQUEST_CODE)
  }

  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    super.onActivityResult(requestCode, resultCode, data)
    when (requestCode) {
      TEST_LOOP_REQUEST_CODE -> testingTV.text = getString(R.string.test_complete)
    }
  }
}
