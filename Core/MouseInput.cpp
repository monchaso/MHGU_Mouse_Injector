#include "MouseInput.h"
#include <windows.h>
// #include <algorithm> // for min/max
#include <cmath>

MouseInput::MouseInput() {}

void MouseInput::Update() {
  // Basic implementation: Center cursor and read delta
  // For production, use Raw Input API (RegisterRawInputDevices)
}

void MouseInput::GetDelta(float &x, float &y) {
  POINT p;
  GetCursorPos(&p);

  // Assume center is 960x540 (1920x1080 / 2)
  // Or get dynamically from SystemMetrics
  int centerX = 1920 / 2;
  int centerY = 1080 / 2;

  x = (float)(p.x - centerX);
  y = (float)(p.y - centerY);

  // Reset cursor to center
  SetCursorPos(centerX, centerY);
}

void MouseInput::CalculateNewAngles(float currentX, float currentY,
                                    float deltaX, float deltaY, float &outX,
                                    float &outY) {
  // MHGU uses radians approx -1.5 to 1.5 vertical? Or 0 to 6.28 horizontal?
  // Based on user:
  // Y Axis: 212E26A6C61 (Up/Down)
  // X Axis: 212E2658FE4 (Left/Right)

  float sensitivity = 0.005f; // Tunable

  outX = currentX + (deltaX * sensitivity);
  outY = currentY + (deltaY * sensitivity);

  // Clamping logic (Example)
  const float PI = 3.14159f;
  // Pitch Clamp (Up/Down) - usually -PI/2 to PI/2
  if (outY > 1.5f)
    outY = 1.5f;
  if (outY < -1.5f)
    outY = -1.5f;

  // Yaw Wrap (Left/Right) - 0 to 2PI
  if (outX > PI * 2)
    outX -= PI * 2;
  if (outX < 0)
    outX += PI * 2;
}
