#pragma once

class TubeDistortion {
public:
  TubeDistortion();
  void setDrive(float drive);
  void setMix(float mix);
  void setEnabled(bool on);
  bool isEnabled() const;
  float drive() const { return drive_; }
  float process(float input);

private:
  static constexpr float kMinimumDrive = 0.1f;
  static constexpr float kMinimumAudibleDrive = 1.0f;
  static constexpr float kDefaultAudibleDrive = 8.0f;

  void updateCompensation();

  float drive_;
  float mix_;
  float cachedComp_;
  bool enabled_;
};
