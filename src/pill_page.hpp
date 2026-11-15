// The chat box over a car: the game's own name tag page (media\UI.zip, Scenes/Anthem/AR_OpponentLabel.xaml, one
// page object per name tag) with a box added over the tag. No game or Windows in here, so the test can run it.
// (The code and the ini keys still say "pill": it began as Forza Horizon 5's round-ended one.)
//
// The box is a plain light rectangle (#F0F0F0) with dark text (#353535), square like the name tag under it, at
// least 350 wide and 164 high, the text 60 in from each side. It moves as Forza Horizon 5's chat pill does: it pops
// up from its bottom edge (scale 0.16 to 1 in 0.63 s, with an overshoot), stays, and from 2 s fades while it rises
// 64, gone at 2.42 s (FH5's A_Storyboard_PopChatBox).
//
// Everything used is markup the game's own pages use (a bad page stops the game when it's loaded):
//   - a ControlTemplate with a DataTrigger that starts a Storyboard on its named element: Controls/HUDSkillTokenControl.xaml;
//   - BackEase and QuadraticEase resources: Controls/HUDDriveToEventControl.xaml, Controls/HUDNetworkStatusControl.xaml;
//   - the Control with a template, StringVisConverter: this page itself.
//
// Two texts feed it, both QuickChat's page texts (FH2 Mod Platform API 11):
//   FH2Mod_Text_QuickChat_Pill     the phrase on this tag, or nothing
//   FH2Mod_Text_QuickChat_PillOn   "1" or "2" while this tag has a box (they take turns per message, so a new
//                                  message on a tag that still shows one pops again), else nothing
#pragma once

#include <string>

namespace qc {

inline constexpr char kPillPagePath[] = "Scenes/Anthem/AR_OpponentLabel.xaml";   // in UI.zip; set_file takes "UI/" + this
inline constexpr char kPillText[]     = "QuickChat_Pill";
inline constexpr char kPillOn[]       = "QuickChat_PillOn";

namespace pill_xaml {

// The box's shape and text: a plain light rectangle that grows with the text.
inline constexpr char kBody[] = R"X(
                <local:FZMaterial Color="#FFF0F0F0" />
                <local:FZTextBlock Text="{Binding FH2Mod_Text_QuickChat_Pill}"
                                   Style="{StaticResource A_Style_FZTextBlock_AnthemA}"
                                   Color="#FF353535"
                                   FontSize="110"
                                   Margin="60,0,60,0"
                                   HorizontalAlignment="Center"
                                   VerticalAlignment="Center" />
)X";

// One trigger: the box's life, started when PillOn becomes `value`.
inline std::string trigger(const char* value) {
    return std::string(R"X(
                <DataTrigger Binding="{Binding FH2Mod_Text_QuickChat_PillOn}" Value=")X") + value + R"X(">
                    <DataTrigger.EnterActions>
                        <BeginStoryboard>
                            <Storyboard>
                                <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.Opacity)" Storyboard.TargetName="ChatPill">
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.0" Value="0"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.03" Value="1"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:2.0" Value="1"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:2.42" Value="0" EasingFunction="{StaticResource QuickChat_Ease_Out}"/>
                                </DoubleAnimationUsingKeyFrames>
                                <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.RenderTransform).(TransformGroup.Children)[0].(ScaleTransform.ScaleX)" Storyboard.TargetName="ChatPill">
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.0" Value="0.16"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.63" Value="1" EasingFunction="{StaticResource QuickChat_Ease_Pop}"/>
                                </DoubleAnimationUsingKeyFrames>
                                <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.RenderTransform).(TransformGroup.Children)[0].(ScaleTransform.ScaleY)" Storyboard.TargetName="ChatPill">
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.0" Value="0.16"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.63" Value="1" EasingFunction="{StaticResource QuickChat_Ease_Pop}"/>
                                </DoubleAnimationUsingKeyFrames>
                                <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.Y)" Storyboard.TargetName="ChatPill">
                                    <EasingDoubleKeyFrame KeyTime="0:0:0.0" Value="0"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:2.0" Value="0"/>
                                    <EasingDoubleKeyFrame KeyTime="0:0:2.42" Value="-64" EasingFunction="{StaticResource QuickChat_Ease_Out}"/>
                                </DoubleAnimationUsingKeyFrames>
                            </Storyboard>
                        </BeginStoryboard>
                    </DataTrigger.EnterActions>
                </DataTrigger>
)X";
}

// Into the page's resources: the two easings and the box as a template (hidden until a trigger starts it).
inline std::string resources() {
    return std::string(R"X(
        <BackEase x:Key="QuickChat_Ease_Pop" EasingMode="EaseOut" Amplitude="0.5"/>
        <QuadraticEase x:Key="QuickChat_Ease_Out" EasingMode="EaseIn"/>
        <ControlTemplate x:Key="QuickChat_ControlTemplate_Pill"
                         TargetType="{x:Type Control}">
            <Grid x:Name="ChatPill"
                  MinWidth="350"
                  Height="164"
                  HorizontalAlignment="Center"
                  RenderTransformOrigin="0.5,1"
                  Opacity="0">
                <Grid.RenderTransform>
                    <TransformGroup>
                        <ScaleTransform/>
                        <SkewTransform/>
                        <RotateTransform/>
                        <TranslateTransform/>
                    </TransformGroup>
                </Grid.RenderTransform>
)X") + kBody + R"X(            </Grid>
            <ControlTemplate.Triggers>
)X" + trigger("1") + trigger("2") + R"X(            </ControlTemplate.Triggers>
        </ControlTemplate>
)X";
}

// Into the page, over the tag: the template's place, or (plain) the box itself, shown while it has a text.
inline constexpr char kPlacePop[] = R"X(
            <Control Template="{StaticResource QuickChat_ControlTemplate_Pill}"
                     HorizontalAlignment="Center"
                     VerticalAlignment="Top"
                     Margin="0,-200,0,0" />
)X";
inline std::string place_plain() {
    return std::string(R"X(
            <Grid HorizontalAlignment="Center"
                  VerticalAlignment="Top"
                  Margin="0,-200,0,0"
                  MinWidth="350"
                  Height="164"
                  Visibility="{Binding FH2Mod_Text_QuickChat_Pill, Converter={StaticResource A_Converter_StringToVis}}"
                  x:Name="ChatPill">
)X") + kBody + R"X(            </Grid>
)X";
}

} // namespace pill_xaml

// The game's name tag page with the box added. `pop`: the box pops up and fades by itself (FH5's animation);
// without it the box is simply there while the tag has a text. Empty, with `why`, if the page isn't the one
// expected.
inline std::string with_pill(const std::string& page, bool pop, std::string* why = nullptr) {
    auto fail = [&](const char* w) {
        if (why) *why = w;
        return std::string();
    };
    if (page.find("FH2Mod_Text_QuickChat_Pill") != std::string::npos) return fail("it already has the box");
    // after the row of icons over the tag (PhotoIcon, HVTIcon), still inside Grid_Container
    const std::string icon = "x:Name=\"HVTIcon\" />";
    const size_t at        = page.find(icon);
    if (at == std::string::npos || page.find(icon, at + 1) != std::string::npos) return fail("the row of icons over the tag isn't there");
    const std::string row_end = "</StackPanel>";
    size_t place              = page.find(row_end, at);
    if (place == std::string::npos) return fail("the row of icons over the tag doesn't end");
    place += row_end.size();
    const std::string res_end = "</Page.Resources>";
    const size_t res          = page.find(res_end);
    if (res == std::string::npos || res > at || page.find(res_end, res + 1) != std::string::npos) return fail("the page's resources aren't there");
    if (page.find("x:Key=\"A_Converter_StringToVis\"") == std::string::npos) return fail("the page's text-to-visibility converter isn't there");
    std::string out = page;
    out.insert(place, pop ? std::string(pill_xaml::kPlacePop) : pill_xaml::place_plain());   // the later place first
    if (pop) out.insert(res, pill_xaml::resources());
    return out;
}

} // namespace qc
