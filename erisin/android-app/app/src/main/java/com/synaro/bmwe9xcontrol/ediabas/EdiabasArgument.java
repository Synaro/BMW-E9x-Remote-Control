package com.synaro.bmwe9xcontrol.ediabas;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Metadata read from an imported SGBD; never inferred from a marketing label. */
public final class EdiabasArgument {
    public final String name;
    public final String description;
    public final Double minimum;
    public final Double maximum;
    public final List<String> documentedValues;

    public EdiabasArgument(String name, String description, Double minimum, Double maximum,
                           List<String> documentedValues) {
        this.name = name == null ? "" : name;
        this.description = description == null ? "" : description;
        this.minimum = minimum;
        this.maximum = maximum;
        this.documentedValues = Collections.unmodifiableList(new ArrayList<>(documentedValues));
    }
}
