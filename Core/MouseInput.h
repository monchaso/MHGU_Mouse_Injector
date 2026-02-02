#pragma once

class MouseInput {
public:
  MouseInput();

  // Updates internal state, usually called every frame
  void Update();

  // Returns the delta movement since last update
  void GetDelta(float &x, float &y);

  // Core Math
  // x, y: Addresses of the camera axis
  // sensitivity: multiplier
  // outputX, outputY: new values to flip/write
  void CalculateNewAngles(float currentX, float currentY, float deltaX,
                          float deltaY, float &outX, float &outY);
};
