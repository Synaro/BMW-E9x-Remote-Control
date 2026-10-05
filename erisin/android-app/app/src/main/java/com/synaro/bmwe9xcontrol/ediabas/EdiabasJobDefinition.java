package com.synaro.bmwe9xcontrol.ediabas;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class EdiabasJobDefinition {
    public final String name;
    public final String description;
    public final List<EdiabasArgument> arguments;

    public EdiabasJobDefinition(String name, String description, List<EdiabasArgument> arguments) {
        this.name = name == null ? "" : name;
        this.description = description == null ? "" : description;
        this.arguments = Collections.unmodifiableList(new ArrayList<>(arguments));
    }
}
