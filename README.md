# ggd-libre

ggd-libre

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
