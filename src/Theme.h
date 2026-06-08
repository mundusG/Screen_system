#ifndef THEME_H
#define THEME_H

#include <QColor>

namespace Theme {

// Background
constexpr int BgR = 10, BgG = 14, BgB = 26;
inline QColor background()       { return QColor(BgR, BgG, BgB); }
inline QColor backgroundAlt()    { return QColor(12, 20, 40); }
inline QColor panelBg()          { return QColor(12, 20, 40, 220); }
inline QColor cardBg()           { return QColor(16, 26, 50, 200); }

// Accent
inline QColor accent()           { return QColor(0, 212, 255); }
inline QColor accentDim()        { return QColor(15, 52, 96); }
inline QColor accentGlow()       { return QColor(0, 180, 255, 40); }
inline QColor accentBorder()     { return QColor(0, 180, 255, 60); }
inline QColor accentBorderBright() { return QColor(0, 212, 255, 140); }

// Status
inline QColor liveGreen()        { return QColor(0, 230, 118); }
inline QColor onlineGreen()      { return QColor(0, 200, 83); }
inline QColor offlineGray()      { return QColor(100, 110, 130); }
inline QColor alertRed()         { return QColor(255, 82, 82); }
inline QColor warningOrange()    { return QColor(255, 171, 64); }

// Text
inline QColor textPrimary()      { return QColor(224, 240, 255); }
inline QColor textSecondary()    { return QColor(160, 200, 255, 180); }
inline QColor textMuted()        { return QColor(100, 130, 170); }

// Borders and separators
inline QColor borderSubtle()     { return QColor(40, 60, 100, 80); }
inline QColor borderMedium()     { return QColor(60, 80, 120); }
inline QColor separator()        { return QColor(40, 60, 100, 100); }

// Video tile
inline QColor tileBg()           { return QColor(14, 18, 30, 220); }
inline QColor tileBorderNormal() { return QColor(30, 50, 80); }
inline QColor tileBorderSelected() { return QColor(0, 212, 255, 160); }
inline QColor tileOverlayBg()    { return QColor(0, 0, 0, 140); }

// Sidebar
constexpr int SidebarWidth = 56;
inline QColor sidebarBg()        { return QColor(8, 12, 24, 240); }
inline QColor sidebarHover()     { return QColor(20, 40, 80); }
inline QColor sidebarActive()    { return QColor(0, 212, 255, 30); }

// Bottom bar
constexpr int BottomBarHeight = 48;
inline QColor bottomBarBg()      { return QColor(8, 12, 24, 240); }
inline QColor buttonBg()         { return QColor(20, 35, 65); }
inline QColor buttonHover()      { return QColor(30, 55, 95); }
inline QColor buttonActive()     { return QColor(0, 140, 200); }

// Right panel
constexpr int RightPanelWidth = 260;

// Grid spacing
constexpr int GridSpacing = 3;
constexpr int LayoutMargin = 4;

} // namespace Theme

#endif // THEME_H
