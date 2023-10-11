#pragma once

struct chapter {
    const char *title;
    const char *url;
};

struct metadata {
    const char *title;
    const char *thumbnail;
    unsigned nb_chapters;
    const struct chapter *chapters;
};

int download(const struct metadata *meta, const char *output_filename);
