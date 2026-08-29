# Lahore Urban Heat Digital Twin — Project Status Report
**State as of Phase 24 In Progress (Post Command-Center & What-If Upgrade)**

> NOTE: Phase 24 (Data Visualization & Executive Charts) marked IN PROGRESS — started 2026-08-28

---

## 1. Executive Summary

The **Lahore Urban Heat Digital Twin** is an interactive, spatial decision-support platform engineered in C++17 and OpenGL 3.3+. It provides urban planners, government officials, and environmental scientists with real-time 3D spatial visualization, microclimate modelling, vulnerability ranking, and multi-hazard scenario simulation (heatwaves, flooding, and green interventions) for Lahore, Pakistan.

The operational decision workflow is:
$$\text{DETECT} \longrightarrow \text{UNDERSTAND} \longrightarrow \text{PRIORITIZE} \longrightarrow \text{SIMULATE} \longrightarrow \text{INTERVENE} \longrightarrow \text{COMPARE} \longrightarrow \text{DECIDE}$$

---

## 2. Architecture & Technology Stack

- **Graphics & Core Engine**: Native C++17, OpenGL 3.3+ (Core Profile), GLFW 3, GLEW, GLM
- **Command-Center UI**: Dear ImGui with custom dark mission-control docked layout, slate/navy palette, and zero window overlap.
- **Data & GIS Pipeline**: Python 3 (OSM spatial parsing, Open-Meteo live weather API, WorldPop headcount estimator, land-cover proxy processing)
- **Data Interchange**: Normalized JSON schemas (`zones.json`, `buildings.json`, `roads.json`, `green_areas.json`, `weather.json`, `population.json`, `satellite_environment.json`)

---

## 3. Implemented Features (Phases 0 — 23)

### Phase 0 — Core C++ / OpenGL 3.3+ Engine Foundation
- Native GLFW window creation, OpenGL 3.3 context initialization, GLEW loading, GLM linear algebra.
- Shader pipeline (`shaders/basic.vert`, `shaders/basic.frag`) with vertex attribute binding.
- Procedural 3D ground plane grid (`MakeGroundGrid`).
- Real-time delta-time updates and FPS free-fly camera with WASD navigation.

### Phase 1 — Project Architecture & Modular C++ Layout
- Decoupled modular design split across `core`, `gis`, `twin`, `ui`, and `simulation`.
- Logging subsystem (`Log.h` / `LogInfo`, `LogWarn`, `LogError`).
- Build configuration using CMake and Visual Studio project filters (`TwinCity3D.vcxproj`).

### Phase 2 — Spatial Grid & GIS Preprocessing Pipeline
- Python preprocessing pipeline (`preprocess_osm.py`, `coordinate_utils.py`) converting geographical coordinates into local meters.
- 4x4 spatial zone grid partitioning the study area (`zones.json`).
- Data structures for zone bounding boxes, building footprints, road polylines, and green polygon geometry.

### Phase 3 — 3D City Scene Rendering Engine
- Procedural extrusion of building footprints into 3D geometry with flat roofs and vertical walls (`CityMeshBuilder`).
- Road network ribbon mesh generation.
- Green area polygon triangulation (`Triangulate.h`) and facility marker generation.
- Merged single-VAO batching for buildings, roads, green areas, and facilities for performance budget optimization.

### Phase 4 — Digital Twin Core Data Model & Spatial Metrics
- `Zone` data model (`Zone.h`) with strict separation between raw data, derived indicators, and placeholder flags.
- Geometry-derived building density (`buildingDensity`) and green coverage (`greenCoverage`) calculated from GIS geometry area ratios.
- Phase 6 derived indicators: exposed surface ratio (`exposedSurfaceRatio`) and environmental heat burden (`environmentalHeatBurden`).

### Phase 5 — Weather Ingestion Pipeline
- Python weather ingestion script (`fetch_weather.py`) polling Open-Meteo API for live temperature, apparent temperature, humidity, wind, and precipitation.
- Offline fallback cache (`weather.json`).
- `WeatherLoader` C++ parser and `DigitalTwin::ApplyWeather` integration with placeholder validation.

### Phase 6 — Environmental Heat Burden & Spatial Microclimate Spread
- Microclimate spread algorithm distributing area-wide weather readings across zones based on relative environmental heat burden.
- Denser, paved, vegetation-poor zones read warmer, while leafy zones read cooler.
- Explicit labelling as a *modelled temperature spread*, strictly avoiding false claims of per-zone physical sensors.

### Phase 7 — Population Exposure Ingestion Pipeline
- Python population estimator (`estimate_population.py`) aggregating WorldPop / OSM building footprint area to assign resident headcount and density (`populationDensity`).
- `PopulationLoader` C++ module and `DigitalTwin::ApplyPopulation` integration.
- Strict placeholder enforcement: invalid data leaves fields flagged as placeholders.

### Phase 8 — Explainable Heat Risk Model
- Transparent composite scoring model (`HeatRiskModel`):
  $$\text{Heat Risk} = 0.35 \times \text{Temp} + 0.20 \times \text{Green Deficit} + 0.20 \times \text{Density} + 0.25 \times \text{Pop Exposure}$$
- Normalized 0–100 score and classification: `Low` (0–30), `Moderate` (31–50), `High` (51–70), `Very High` (71–85), `Extreme` (86–100).
- Fully explainable parameters; skipped on incomplete inputs.

### Phase 9 — Multi-Layer Visualization Engine & Shader Color Ramps
- `DataLayer` enum enabling dynamic color-coding of 3D city buildings and green spaces.
- Dynamic GPU color mapping (`DataRamp` in `basic.frag`): Blue $\rightarrow$ Green $\rightarrow$ Yellow $\rightarrow$ Orange $\rightarrow$ Red.
- Keyboard layer switching shortcuts (`[1-9]` and `[L]` to cycle).

### Phase 10 — Interactive 3D Selection & Zone Inspector UI
- Ray-casting picking pipeline (`Picking.h`/`.cpp`) unprojecting screen-center / cursor ray to select zones.
- Dear ImGui integration (`imgui_impl_glfw`, `imgui_impl_opengl3`).
- On-screen `Zone Inspector` panel showing selected zone metrics, risk band, and contribution factors.

### Phase 11 — Priority Engine & Top Priority Panel
- Decision-support ranking engine (`PriorityModel`):
  $$\text{Priority} = 0.45 \times \text{Heat Risk} + 0.25 \times \text{Pop Headcount} + 0.15 \times \text{Env Burden} + 0.15 \times \text{Pop Exposure}$$
- Ranks zones #1 to #N for government intervention.
- Interactive `Top Priority Zones` UI panel; clicking any ranked zone snaps the camera to focus on it and opens the inspector.

#### PHASE 11 STATUS REPORT

PHASE STATUS: Completed

FILES CREATED/MODIFIED:
- src/twin/PriorityModel.h / PriorityModel.cpp — priority scoring and ranking implementation
- src/twin/Zone.h — priority fields (priority, priorityIsPlaceholder, priorityRank)
- src/twin/Digitaltwin.cpp — ComputePriority integration and DigitalTwin::ComputePriority call sites
- src/ui/Inspector.cpp — Top Priority panel + WHY breakdown
- src/ui/CommandCenterUI.cpp — Right inspector Top Priority list and scene integration
- src/core/Application.cpp — FocusCameraOnZone and wiring of UI focusedZoneId

IMPLEMENTED:
- A normalized 0–100 priority score combining heat risk, population magnitude, environmental burden, and population exposure.
- Ranking (1 = highest priority) and clickable Top Priority UI that focuses camera, highlights zone, and opens the inspector showing "WHY this zone".

TEST RESULT:
- Build successful (x64 Debug).
- Manual verification steps: load app, ensure zones show non-placeholder priorities after population + temperature data are applied; open "Top Priority Zones" list, click an entry and confirm camera focuses, zone highlights, and Inspector shows priority breakdown.

KNOWN LIMITATIONS:
- Priority is a prototype decision-support indicator and is explicitly labelled as such in the UI. It is relative to the study area and depends on availability of temperature and population data per zone.
- Weighting is configurable in PriorityWeights but no UI slider exists yet to retune weights live (Phase 31 / Phase 29 planned).

NEXT PHASE: Phase 12 — Professional 3D Digital Twin Visualization (polish building materials, camera transitions, and facility markers).

### Phase 12 — Multi-Mode Professional Camera System
- Four distinct camera modes (`CameraMode` in `Camera.h`):
  1. **FreeFly**: FPS-style WASD navigation with mouse-look.
  2. **Orbit**: Target-focused orbit with drag rotation, right-drag panning, and scroll dolly.
  3. **TopDown**: Orthographic-style 2D map view.
  4. **Isometric**: Fixed 45-degree angle architectural view.
- Smooth camera position and target interpolation (`FocusOn` / `Tick`).
- Dynamic cursor capture management (`ApplyCursorModeForCurrentCamera`) with hotkeys `[F]`, `[O]`, `[T]`, `[I]`, and `[R]`.

### Phase 13 — Interactive City Layers Control Panel
- Single-select layer switching directly from the UI, fully synchronized with keyboard hotkeys.

### Phase 14 — Selected Zone 3D Wireframe Highlight & Dynamic Legend
- Dynamic 3D wireframe mesh (`SelectionHighlight`) built around the selected zone boundary with pulse animation.
- On-screen `Legend` UI panel (`HeatLegend`) displaying active layer metrics and color ramp bands.

### Phase 15 — Heatwave Scenario Engine & Dynamic Dashboard Controls
- Real-time scenario engine (`ScenarioState`). Presets (`+1°C` to `+5°C`) and custom slider for simulated heatwave conditions.
- Real-time recalculation of zone temperatures, heat risks, affected populations, and priority rankings upon scenario modification.

### Phase 16 — Flood Risk Prototype Scenario
- Heavy rainfall scenario simulator (`80 mm` and `120 mm` presets and slider). Surface susceptibility proxy combining building density, exposed ground ratio, and green coverage deficit.

### Phase 17 — Population Exposure Metrics
- Risk-weighted population exposure indicators: `heatExposedPopulation`, `highRiskPopulation` ($\ge 70$ risk), `extremeRiskPopulation` ($\ge 85$ risk).

### Phase 18 — Satellite Environmental Data Layer
- `SatelliteEnvironmentLoader` ingestion of preprocessed raster / land-cover proxies (`satellite_environment.json`). Vegetation (`vegetationIndex`) and built-up (`builtUpIndex`) layer.

### Phase 19 — Green Infrastructure Analysis & Priority Engine
- Dedicated `GreenInfrastructureModel` answering **WHERE to add greenery**:
  - `greenDeficit`: Gap between current green coverage and the 20% urban target.
  - `greenPriority`: Composite score (0–100) combining Green Deficit (40%), Heat Risk (35%), and Population Exposure (25%).
  - `greenPriorityRank`: #1 to #N ranking of green intervention urgency.
  - `greenBenefitScore`: Modelled heat-risk reduction potential estimate.
- `Green Priority` 3D visualization layer (`DataLayer::GreenPriority`, hotkey `[9]`).

### Phase 20 — What-If Intervention Engine
- Planner-oriented Urban Intervention simulator (`ApplyInterventionScenario`):
  - **Vegetation Increase**: `+5%`, `+10%`, `+15%`, `+20%` presets & custom slider.
  - **Shade & Reflective Roofing**: `+5%`, `+10%`, `+20%` presets & custom slider.
  - **Intervention Scope**: City-Wide vs Selected Zone Target (`Apply to Selected Zone Only`).
- Immediate re-scoring of microclimate heat burden, heat risk score, and affected population.

### Phase 21 — Comparative Before / After Mode
- Dynamic comparison engine storing `baselineHeatRisk`, `baselinePriority`, and `baselineGreenCoverage`.
- Comparative Card displaying `BEFORE` vs `AFTER` vs `CHANGE (-18 pts)`.
- Zone Inspector display showing zone-level intervention delta points.
- Mandatory `"MODELLED SCENARIO ESTIMATE"` label.

### Phase 22 — Multi-Layer City Command Center Layout
- Overhaul of UI from separate floating windows into a **Unified Docked Mission Control Dashboard**:
  - **Top Header Bar**: Title, live status badge (`● LIVE`), real-time temperature, heatwave warning badge, timestamp, camera mode buttons (`[FreeFly]`, `[Orbit]`, `[2D]`, `[Iso]`).
  - **Left Navigation Sidebar**: Docked menu with 9 city visualization layers, scenario tab, and study area inset.
  - **Right Inspector Panel**: Docked zone intelligence inspector (Heat Risk Card, metrics grid, intervention delta, why-this-zone breakdown) and Top Priority Zones list with progress bars.
  - **Bottom Analytics Dashboard**: Horizontal dock across bottom center housing Legend, Risk Distribution, Exposed Population, Green Infrastructure progress, and Scenario & What-If controls.
  - **Bottom Status Footer**: Data sources (`Open-Meteo | OpenStreetMap | WorldPop | Copernicus`) and update timestamp.

### Phase 23 — Professional UI / UX Command Center Aesthetics
- Custom dark command-center theme (`CommandCenterUI::InitStyle`):
  - Slate background (`#0b1017`), dark navy container panels (`#101722`), cyan/green/red accent badges.
  - ImGui namespace isolation (`PushID("vegControls")` / `PushID("shadeControls")`) fixing widget ID collisions.
  - Zero text overlap across all display resolutions.

---

## 4. Remaining Features Roadmap (Phases 24 — 41)

```
       IMPLEMENTED (Phases 0-23)              REMAINING (Phases 24-41)
[✓] Core OpenGL Engine & City Mesh     |  [>] Phase 24: Data Charts & Sparklines
[✓] GIS Preprocessing & Grid           |  [ ] Phase 25: Light Pollution & Biodiversity
[✓] Weather & Population Pipelines    |  [ ] Phase 26: Air Quality Layer (OpenAQ)
[✓] Heat Risk & Priority Models        |  [ ] Phase 27: Citizen Reporting Layer
[✓] Multi-Layer Shaders & Camera       |  [ ] Phase 28: AI Forecasting Engine
[✓] Scenarios (Heatwave & Flood)      |  [ ] Phase 29: Explainable Decision Engine
[✓] Satellite & Green Infra Layer      |  [ ] Phase 30: Strategic City Command View
[✓] What-If Intervention Engine        |  [ ] Phase 31: Unified Scenario Control
[✓] Before / After Comparative Delta   |  [ ] Phase 32: GPU Visual FX & Shaders
[✓] Unified Command Center Dashboard   |  [ ] Phase 33: GPU Performance Optimization
[✓] Professional UI/UX Styling         |  [ ] Phase 34: Final Visual Quality Pass
                                       |  [ ] Phase 35: Deterministic Pitch Demo
                                       |  [ ] Phase 36: Data Credibility & Badges
                                       |  [ ] Phase 37: Extensible Govt Abstraction
                                       |  [ ] Phase 38: Final Command Structure
                                       |  [ ] Phase 39: Government Decision Card
                                       |  [ ] Phase 40: Code Cleanup & Audit
                                       |  [ ] Phase 41: Acceptance & Hackathon Check
```

### Detail of Upcoming Phases:

#### Phase 24 — Data Visualization & Executive Charts
- Integrated analytical graphics (sparklines, risk distribution pie/donut charts, population exposure breakdown bars).

**Status:** IN PROGRESS — started 2026-08-28

#### Phase 25 — Light Pollution & Bird/Ecological Impact Layer
- Night-light satellite layer and ecological disturbance indicator highlighting overlaps between high light pollution, green space, and bird habitats.

#### Phase 26 — Air Quality Integration
- Ingestion of OpenAQ / AQICN station data (AQI, PM2.5) overlaid onto the 3D twin.

#### Phase 27 — Citizen Reporting Layer Prototype
- Pinboard prototype on 3D map for crowd-sourced reports (Heat, Flood, Waste, Broken Drainage) with status tracking (`NEW`, `VERIFIED`, `IN PROGRESS`, `RESOLVED`).

#### Phase 28 — AI / Statistical Forecasting Layer
- Transparent statistical forecasting model predicting future temperature and exposure trends.

#### Phase 29 — Explainable Decision Engine ("WHY IS THIS ZONE HIGH PRIORITY?")
- Dedicated breakdown panel decomposing priority score into visual contribution bars and outputting actionable planner recommendations.

#### Phase 30 — Strategic City-Wide Command View
- Macro view providing regional Lahore overview while maintaining neighbourhood detail in the focused digital twin.

#### Phase 31 — Unified Scenario Control Center
- Unified interface combining Heatwave, Flood, and What-If Interventions into single multi-hazard scenario runs.

#### Phases 32 — 34 — GPU Visual Effects, Optimization & Visual Polish
- GPU shader enhancements (bloom, fog, selection glow), mesh instancing, frustum culling, and visual refinement pass.

#### Phase 35 — Deterministic Hackathon Presentation Demo Mode
- Automated 2–3 minute guided cinematic flythrough demonstrating the full DETECT $\rightarrow$ UNDERSTAND $\rightarrow$ PRIORITIZE $\rightarrow$ SIMULATE $\rightarrow$ INTERVENE $\rightarrow$ COMPARE $\rightarrow$ DECIDE pipeline.

#### Phases 36 — 41 — Credibility, Architecture, Final Pitch & Acceptance
- Data source badge tags, extensible C++ `IDataSource` abstraction layer for government feeds (EPA, Suthra Punjab), final decision summary card, engineering cleanup, and acceptance testing.

---

## 5. Codebase Summary Matrix

| File / Component | Role & Functionality | Implemented Phase |
| :--- | :--- | :--- |
| `src/core/Application.h / .cpp` | Window lifecycle, render loop, event routing, scenario updates | Phase 0–23 |
| `src/core/Camera.h` | 4-mode camera (FreeFly, Orbit, TopDown, Isometric) & smooth transitions | Phase 0, 12 |
| `src/core/Renderer.h / .cpp` | OpenGL frame setup, state management, clear calls | Phase 0 |
| `src/core/Shader.h / .cpp` | Shader loader and uniform binding | Phase 0 |
| `src/core/Mesh.h / .cpp` | VBO/VAO creation and indexed draw calls | Phase 0, 3 |
| `src/core/Picking.h / .cpp` | Ray casting and ground unprojection for zone picking | Phase 10 |
| `src/gis/GISDataset.h / GISTypes.h` | Data structures for zone bounds, buildings, roads, green areas | Phase 2 |
| `src/gis/GISLoader.h / .cpp` | Ingestion of preprocessed JSON spatial files | Phase 2 |
| `src/gis/Triangulate.h` | Polygon triangulation for green spaces & roofs | Phase 3 |
| `src/twin/Zone.h` | Core data model (density, heat risk, priority, green priority, baseline metrics) | Phase 4–21 |
| `src/twin/DigitalTwin.h / .cpp` | Digital Twin manager, microclimate spread, intervention engine | Phase 4–21 |
| `src/twin/CityMeshBuilder.h / .cpp` | 3D geometry extrusion and vertex color baking | Phase 3, 9 |
| `src/twin/HeatRiskModel.h / .cpp` | Explainable heat risk score computation engine | Phase 8 |
| `src/twin/PriorityModel.h / .cpp` | Government intervention ranking model | Phase 11 |
| `src/twin/GreenInfrastructureModel.h / .cpp` | Green deficit & WHERE-to-add-greenery priority engine | Phase 19 |
| `src/twin/HeatLayers.h / .cpp` | DataLayer normalization & DescribeLayer metadata | Phase 9, 18, 19 |
| `src/twin/SelectionHighlight.h / .cpp` | Dynamic 3D wireframe mesh for selected zone | Phase 14 |
| `src/ui/CommandCenterUI.h / .cpp` | Unified docked Mission Control Dashboard (Top, Left, Right, Bottom, Footer) | Phase 22, 23 |
| `src/ui/Inspector.h / .cpp` | Dear ImGui Zone Inspector & Top Priority panels (integrated into CommandCenterUI) | Phase 10, 11 |
| `src/ui/LayersPanel.h / .cpp` | City Layers selection UI panel (integrated into CommandCenterUI) | Phase 13, 19 |
| `src/ui/HeatLegend.h / .cpp` | Dynamic gradient ramp & legend UI (integrated into CommandCenterUI) | Phase 14 |
| `src/ui/ScenarioPanel.h / .cpp` | Scenario simulation UI & What-If controls (integrated into CommandCenterUI) | Phase 15, 16, 20 |
| `src/ui/GreenInfrastructurePanel.h / .cpp` | Green infrastructure deficit & priority UI panel | Phase 19 |
| `shaders/basic.vert / basic.frag` | Vertex transformation & data ramp GPU fragment shader | Phase 0, 9 |

---
*Report generated automatically for Lahore Urban Heat Digital Twin Development Trajectory.*
