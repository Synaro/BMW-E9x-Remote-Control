package com.synaro.bmwe9xcontrol.ediabas;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class EdiabasResultDefinition {
    public final String name;
    public final String type;
    public final List<String> comments;

    public EdiabasResultDefinition(String name, String type, List<String> comments) {
        this.name = name == null ? "" : name;
        this.type = type == null ? "" : type;
        this.comments = Collections.unmodifiableList(new ArrayList<>(comments));
    }
}
