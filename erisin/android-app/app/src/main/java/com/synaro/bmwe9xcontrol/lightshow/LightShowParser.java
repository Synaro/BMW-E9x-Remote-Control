package com.synaro.bmwe9xcontrol.lightshow;

import com.google.gson.Gson;
import com.google.gson.JsonParseException;

import java.io.Reader;
import java.util.List;

public final class LightShowParser {
    private LightShowParser() {}

    public static LightShow parse(Reader reader) {
        LightShow show;
        try {
            show = new Gson().fromJson(reader, LightShow.class);
        } catch (JsonParseException ex) {
            throw new IllegalArgumentException("invalid light-show JSON", ex);
        }
        if (show == null) throw new IllegalArgumentException("empty light-show JSON");
        List<String> errors = show.validate();
        if (!errors.isEmpty()) throw new IllegalArgumentException(String.join("; ", errors));
        return show;
    }
}
