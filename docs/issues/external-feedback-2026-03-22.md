# External Feedback Review - 2026-03-22

## Scope

This note summarizes external discussion about the original Tg or tg-timer project outside the upstream GitHub issue tracker.

Sources reviewed included:

- Reddit
- Watch-related forum search results
- Fedora Copr pages and discussion
- package ecosystem metadata

## Overall Impression

The general outside perception is positive about the core idea and practical usefulness of the software.

The recurring concerns are not about the concept itself. They are mostly about:

- microphone quality and setup
- calibration difficulty
- build and packaging friction
- platform-specific runtime issues
- maintenance fragmentation across forks

## Positive Signals

### Reddit: hobbyists view Tg as a practical low-cost alternative

In the Reddit thread `[GUIDE] HOMEMADE TIMEGRAPHER` on r/RepTime, the tone is strongly positive.

Key takeaways:

- Tg is presented as a way to turn an ordinary microphone into a timegrapher.
- The author describes it as simple and usable for hobbyists.
- The post claims repeatable results when using a decent microphone.
- The author explicitly compares it favorably with lower-end dedicated timegrapher hardware.
- Other users respond positively, especially beginners and hobbyists.

Representative perception from that thread:

- good for newcomers
- useful enough to try in practice
- potentially better value than cheap dedicated hardware

### Package ecosystem visibility suggests the tool is still known and used

The software still appears in Linux packaging ecosystems.

Observed signals:

- Fedora Copr package exists under `tpiepho/tg-timer`
- MyNixOS lists `tg-timer` version `0.7.0`
- MyNixOS points to `https://github.com/xyzzy42/tg` as homepage rather than the original repository

This suggests the software remains visible and relevant enough for downstream packagers, but that the center of gravity has shifted away from the original upstream.

## Recurring Complaints And Friction

### 1. Microphone quality matters a lot

This is the most repeated practical caveat.

External feedback consistently suggests:

- built-in laptop microphones are not ideal
- earbud microphones can sometimes work surprisingly well
- better microphones improve repeatability significantly
- the watch must be positioned very close to the microphone
- quiet environment matters for calibration and measurement quality

This aligns with the documentation gap already identified in this repository.

### 2. Calibration is perceived as necessary but not especially user-friendly

The Reddit guide spends a substantial amount of effort explaining calibration.

Observed themes:

- users need a clear external timing reference
- calibration can take time and may require repeated attempts
- users do not intuitively understand why software needs calibration
- some users ask basic conceptual questions that indicate the app is not self-explanatory on first use

This suggests the current project would benefit from better onboarding and clearer explanation of why calibration exists.

### 3. Build and packaging quality look uneven across ecosystems

There are clear signs that packaging exists, but the maintenance story is fragmented.

Evidence:

- Fedora Discussion shows an automatically generated package page with a warning-like installation note: `Author knows what to do. Everybody else should avoid this repo.`
- The Fedora Copr project has package downloads across several Fedora and EPEL releases, which indicates real usage.
- The same Copr page reports the last build succeeded three years ago, which is a maintenance warning sign.

Interpretation:

- There is real downstream interest.
- The packaging story exists but does not look actively curated from the original upstream.

### 4. Maintenance appears fork-driven rather than upstream-driven

Multiple external references point to forks rather than the original repository.

Examples:

- GitHub issues in the original repo repeatedly refer users to `xyzzy42/tg`
- MyNixOS uses `xyzzy42/tg` as homepage
- Fedora Copr contact links and package presence point outside the original upstream context

Interpretation:

- The original repo created the user base and reputation.
- Community maintenance energy seems to have moved into forks.
- From the outside, this can make the project look alive but organizationally unclear.

## Forum Signals

### Watch forums appear to have sustained long-term discussion

Direct forum extraction was partially blocked, but search-result snippets still show strong signals from long-running WatchUseek threads.

Topics visible from search snippets include:

- calibration procedure discussion
- Windows compilation and runtime problems
- macOS microphone issues
- deeper algorithm questions about lock and unlock detection

Interpretation:

- The software generated meaningful technical discussion, not just curiosity.
- Users treat it as a serious tool worth troubleshooting and understanding.

## Practical Reputation Summary

If someone asks "what does the internet seem to think about tg-timer?", the short answer is:

- The idea is respected.
- The tool is seen as genuinely useful.
- It is especially attractive to hobbyists because it lowers the barrier to having a timegrapher.
- People do get real results from it.
- The weak points are setup complexity, calibration, platform quirks, and maintenance fragmentation.

## What This Means For This Repository

The external feedback reinforces several conclusions from the local audit:

1. The software itself is worth maintaining.
2. Documentation is a high-leverage improvement area.
3. Microphone guidance and calibration guidance should be first-class docs.
4. Platform-specific troubleshooting, especially for macOS audio permissions and Windows builds, has real user value.
5. Clarifying the canonical maintained repository matters, because the public story is currently spread across old upstream plus active forks.

## Limits Of This Review

- Some forum pages blocked direct extraction.
- WatchUseek findings were partly inferred from search-result snippets rather than full thread parsing.
- Reddit coverage found one strong, useful thread rather than a large volume of broad discussion.
- This review focused on high-signal public mentions, not exhaustive web coverage.
