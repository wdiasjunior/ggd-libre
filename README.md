# ggd-libre

Reverse engineered CLAP Plugin that reimplements the GGD Kontakt drum plugin.

If you like me use Linux and are interested in music production, you have likely already come across the massive challenge that is using Kontakt libraries through Wine. This project was built with the sole purpose of extracting the sample files from a legally purchased copy of GGD Matt Halpern Signature Pack and use it in a native plugin that runs on Linux and Windows.

ggd-libre has no affiliation to GGD and I do condone piracy of their products. This project simply exists as a way to use the plugin you paid for without relying on proprietary Kontakt bullshit and DRM.

---

## GGD reverse engineer prompt

in this directory I have a kontakt drum plugin with some python scripts that were used to extract the sample file contents and generate midi mappings. I really hate kontakt and how it works, so I want use the extracted files to create my own CLAP plugin in C, reverse engineering the original ggd plugin's functionality using the screenshots "ui-cymbals.png","ui-kick.png","ui-snare.png","ui-toms.png" as a base for the UI and how it should behave with the audio levels for each drum piece sample.

the plugin should behave like any other vst/sampler, taking in midi input and outputting the drum sounds in mono or stereo. the UI should have a tab bar at the top for: kick, snare, toms and cymbals. for each tab, add a slider to control gain level, pan, mute, solo, stereo/mono and phase for each drum piece/sample. if a drum piece has more than one measurement/size like the snare and kick, add a selector to switch between the different sizes. also add a master output level slider.

TODO - add velocity controls - velocity curve?

write me a .md file with an in depth step by step process to achieve this on my own with other kontakt/gdd drum libraries, from end to end using the tools you generated and the clap plugin, since you studied the clap plugin api/sdk. I would like to write synths and guitar effects in the future. also, document in depth in a separate .md file every component of the CLAP plugin you implemented.

---

i want to add another tab to switch between different ggd libs
one kit wonder / matt halpern

but first i want to extract the one kit wonder and map the drum pieces to midi
then i want you to analyze and compare it with the matt halpern samples
if there is a difference between them, let's create a different set of tabs with different options or a different plugin alltogether

---
---
---
---
---

the linux version is working but the windows version fails to load on reaper.
also the midi map labels are not being displayed.

---

I want to rename the vst directory to plugin and make sure git knows I just renamed it, not removed the files and not mess up the git history.


---
## improvements

looking at the screenshots in the root of the repo, we can see that every tab (kick, snare, toms, cymbals) has sliders/faders for the room samples, which are currently missing from the clap plugin

kick tab
- close mic
- oh
- near room
- far room

snare tab
- top mic 1
- top mic 2
- bottom mic
- oh
- near room
- far room

toms tab
- rack 1
- rack 2
- floor 1
- floor 2
- oh
- near room
- far room

cymbals tab
- hi-hat
- ride
- stack
- splash
- china
- oh
- near room
- far room

I am assuming that when I hit a snare all snare related samples should be played, and I would have individual controls for the volume of each snare mic.

I don't see the sliders but is this implemented multiple sample playback feature implemented?

on top of this, just like the kick and snare tabs have a size selector, the screenshots show that every tom and cymbal piece have selectors as well. and for the size selector, I noticed that only the kick selector seems to have an effect on the sound, while the snare all options sound identical so double check that whilst implemententing the other tabs drum piece size/sample seletor

---

can you add a way to reset the fader levels to 0db by double clicking it?
