#pragma once

const int UPDATE_RATE = 60;
const int UPDATE_DELTA = 1000000 / UPDATE_RATE;
const float GRAVITY = -980.0f;

const int NET_UPDATE_DELTA = 4 * UPDATE_DELTA;

const char STATE_MANAGER_KEY[] = "state_manager";
const char INPUT_MANAGER_KEY[] = "input_manager";
const char FLOW_CONTROL_KEY[] = "flow_control";
const char REN_CONTEXT_KEY[] = "ren_context";
const char SND_CONTEXT_KEY[] = "snd_context";
const char RANDOM_KEY[] = "random_eng";
const char LOG_KEY[] = "log";
const char SHADER_LOADER_KEY[] = "shader_loader";

const char UI_RENDERER_KEY[] = "ui_renderer";
const char UI_ROOT_KEY[] = "ui_root";
