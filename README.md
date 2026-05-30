# ggd-libre

Reverse engineered CLAP Plugin that reimplements the GGD Kontakt drum plugin.

If you like me use Linux and are interested in music production, you have likely already come across the massive challenge that is using Kontakt libraries through Wine. This project was built with the sole purpose of extracting the sample files from a legally purchased copy of GGD Matt Halpern Signature Pack and use it in a native plugin that runs on Linux and Windows.

ggd-libre has no affiliation to GGD and I do condone piracy of their products. This project simply exists as a way to use the plugin you paid for without relying on proprietary Kontakt bullshit and DRM.

Disclaimer, this project was entirely vibe coded as an experiment to gauge how well LLMs fare in reverse engineering tasks.

---

## GGD reverse engineer prompt

in this directory I have a kontakt drum plugin with some python scripts that were used to extract the sample file contents and generate midi mappings. I really hate kontakt and how it works, so I want use the extracted files to create my own CLAP plugin in C, reverse engineering the original ggd plugin's functionality using the screenshots "ui-cymbals.png","ui-kick.png","ui-snare.png","ui-toms.png" as a base for the UI and how it should behave with the audio levels for each drum piece sample.

---

i want to add another tab to switch between different ggd libs
one kit wonder / matt halpern

but first i want to extract the one kit wonder and map the drum pieces to midi
then i want you to analyze and compare it with the matt halpern samples
if there is a difference between them, let's create a different set of tabs with different options or a different plugin alltogether

---

I want to rename the vst directory to plugin and make sure git knows I just renamed it, not removed the files and not mess up the git history.

---
