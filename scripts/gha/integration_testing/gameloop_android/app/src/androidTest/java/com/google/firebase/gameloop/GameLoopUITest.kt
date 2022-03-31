package com.google.firebase.gameloop

import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.filters.SdkSuppress
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.platform.app.InstrumentationRegistry.getInstrumentation
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@SdkSuppress(minSdkVersion = 18)
@LargeTest
class GameLoopUITest {

  companion object {
    const val GAMELOOP_TIMEOUT = 20 * 60 * 1000L
  }

  private lateinit var device: UiDevice

  @get:Rule
  var activityRule: ActivityScenarioRule<MainActivity>
          = ActivityScenarioRule(MainActivity::class.java)

  @Before
  fun initDevice() {
    device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
  }

  @Test
  fun testLaunchGameLoop() {
    val element = device.wait(
      Until.findObject(By.text(getInstrumentation().targetContext.getString(R.string.test_complete))),
      GAMELOOP_TIMEOUT)
    assertNotNull(element)
  }
}
