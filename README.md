# thesis-pt-v2a

Research prototype developed as part of the master’s thesis "AI for Sound Design: Integrating Scene-Aware Sound
Generation & Retrieval in Film Post-Production".
The system combines AI-based audio generation, semantic sound recommendation, and spotting assistance in a Pro Tools-native AAX plugin workflow.

## What This Project Does

The project enables three DAW-integrated workflows inside Pro Tools:

1. Audio Generation
Generates sound for a selected scene segment from video and optional text prompt, then inserts the result into the timeline.

2. Sound Recommendation
Retrieves candidate sounds from a searchable library based on video, text, or hybrid queries.

3. Spotting Support
Creates marker-based event suggestions to support spotting in predefined study scenes used for prototype evaluation (Wizard-of-Oz).

## System Overview

The repository is split into a plugin layer and multiple backend services:

- AAX plugin
	- JUCE-based AAX plugin for Pro Tools
	- Handles timeline interaction, user actions, and API calls
	- Embeds Python runtime and helper scripts for integration workflows

- AI generation services
	- MMAudio API for video-conditioned audio generation
	- HunyuanVideo-Foley API for alternative foley generation workflows

- Retrieval service
	- Sound Search API (X-CLIP + pgvector/PostgreSQL)
	- Supports text/video similarity search on BBC sound metadata

- Companion tooling
	- Shared Python modules for Pro Tools integration, API clients, CLI, and video preprocessing

## Demo

### 1. Audio Generation
[Video / GIF here]

### 2. Sound Recommendation
[Video / GIF here]

### 3. Spotting Support
[Video / GIF here]

## Repository Structure

```text
thesis-pt-v2a/
├── aax-plugin/                 # JUCE AAX plugin project, installers, build scripts
├── companion/                  # Shared Python integration and utility modules
├── standalone-API/             # MMAudio API service
├── hunyuanvideo-foley-API/     # HunyuanVideo-Foley API service
├── sound-search-API/           # X-CLIP + pgvector retrieval API
├── shared/                     # Shared configuration helpers
├── db-init/                    # Database initialization scripts
├── external/                   # Third-party code (e.g. JUCE, py-ptsl)
├── CONFIGURATION.md            # Central configuration reference
└── .env.example                # Environment variable template
```

## Architecture at a Glance

1. User selects a video clip in Pro Tools (+ optional text input in the plugin).
2. AAX plugin calls Python helper scripts bundled in plugin resources.
3. Helper scripts communicate with one or more backend APIs.
4. Generated audio or search results are returned.
5. Plugin inserts or references results in the Pro Tools session.


## Setup

This repository contains multiple services and a Pro Tools AAX plugin.  
For configuration details, environment variables, and service setup, see [`CONFIGURATION.md`](CONFIGURATION.md).

## Status

This repository contains a research prototype developed in the context of a master’s thesis.  
It is not intended as a production-ready tool.

## Data & Licensing

This project integrates with the **BBC Sound Effects Archive** for research-only search/retrieval. **No BBC audio or metadata are included in this repository**. Users must obtain any BBC content under the BBC’s licensing terms:

- Archive: https://sound-effects.bbcrewind.co.uk/

## Upstream Models & Repositories

- MMAudio (video-conditioned audio generation): [https://github.com/PKU-YuanGroup/MMAudio](https://github.com/hkchengrex/MMAudio)
- HunyuanVideo-Foley: [https://github.com/Tencent/HunyuanVideo-Foley](https://github.com/Tencent-Hunyuan/HunyuanVideo-Foley)x-clip 
- X-CLIP (retrieval backbone): [https://github.com/microsoft/X-CLIP](https://huggingface.co/microsoft/xclip-base-patch32)

## Citation

If you reference this project, please cite the associated master’s thesis.

## License

See the repository license and the respective third-party component licenses.
