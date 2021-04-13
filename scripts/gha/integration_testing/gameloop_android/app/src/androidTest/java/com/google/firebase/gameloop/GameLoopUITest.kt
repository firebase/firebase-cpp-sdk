package com.google.firebase.gameloop

import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import junit.framework.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@LargeTest
class GameLoopUITest {

  companion object {
    const val COMPLETE_TEST = "Game Loop Complete"
    const val GAMELOOP_TIMEOUT = 3 * 60 * 1000L
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
    val element = device.wait(Until.findObject(By.text(COMPLETE_TEST)), GAMELOOP_TIMEOUT)
    assertNotNull(element)
  }
}