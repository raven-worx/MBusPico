#pragma once

#include "bootstrap_min_css.raw.h"
#include "bootstrap_bundle_min_js.raw.h"
#include "config_html.raw.h"
#include "config_js.raw.h"

struct web_res { const char* url; const unsigned char* data; const unsigned int size; };
const struct web_res g_web_resources[] = {
	{"/bootstrap.min.css", __bootstrap_min_css, __bootstrap_min_css_size},
	{"/bootstrap.bundle.min.js", __bootstrap_bundle_min_js, __bootstrap_bundle_min_js_size},
	{"/config.html", __config_html, __config_html_size},
	{"/config.js", __config_js, __config_js_size},
};