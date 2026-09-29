// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace LuxuryUiSettings {

inline constexpr int kMaxAvatarCorners = 23;
inline constexpr int kMaxMenuRadius = 18;
inline constexpr int kMinMenuRadius = 2;
inline constexpr int kDefaultMenuRadius = 10;

void setMonoFont(QString newFont);
QString getMonoFont();

void setWideMultiplier(double val);
int getWideMultiplied(int width, double mult);

void setMaterialSwitches(bool val);
bool isMaterialSwitches();

void setAvatarCorners(int val);
int getAvatarCorners();

void setMenuRadius(int val);
int getMenuRadius();
// Runtime popup-menu corner radius: one global value overrides every
// style radius; kept inside a sane 2-18px range.
int effectiveMenuRadius(int styleRadius);

}
