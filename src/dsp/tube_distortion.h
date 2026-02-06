#pragma once

class TubeDistortion {
public:
  TubeDistortion();
  void setDrive(float drive);
  void setMix(float mix);
  void setEnabled(bool on);
  bool isEnabled() const;
  float process(float input);

private:
  float drive_;
  float mix_;
  float cachedComp_;
  bool enabled_;
};
