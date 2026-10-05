package com.synaro.bmwe9xcontrol.frm;

import java.util.Locale;

/** A candidate capability derived from job metadata in the user's imported SGBD. */
public final class FrmCapability {
    public final String id;
    public final String label;
    public final String sgbd;
    public final String ediabasJob;
    public final String valueArgument;
    public final boolean supportsOnOff;
    public final boolean supportsPwm;
    public final int minimum;
    public final int maximum;
    public final String evidence;
    private boolean armed;
    private String restoreJob = "";

    public FrmCapability(String id, String label, String sgbd, String ediabasJob,
                         String valueArgument, boolean supportsOnOff, boolean supportsPwm,
                         int minimum, int maximum, String evidence) {
        this.id = required(id, "id");
        this.label = required(label, "label");
        this.sgbd = required(sgbd, "sgbd");
        this.ediabasJob = required(ediabasJob, "ediabasJob");
        this.valueArgument = required(valueArgument, "valueArgument");
        this.supportsOnOff = supportsOnOff;
        this.supportsPwm = supportsPwm;
        this.minimum = minimum;
        this.maximum = maximum;
        this.evidence = evidence == null ? "" : evidence;
        if (minimum > maximum) throw new IllegalArgumentException("minimum exceeds maximum");
    }

    private static String required(String value, String name) {
        if (value == null || value.trim().isEmpty()) throw new IllegalArgumentException(name + " is required");
        return value.trim();
    }

    public boolean isArmed() { return armed; }
    public void setArmed(boolean armed) { this.armed = armed; }
    public String getRestoreJob() { return restoreJob; }
    public void setRestoreJob(String restoreJob) { this.restoreJob = restoreJob == null ? "" : restoreJob.trim(); }
    public boolean canRestoreControl() { return !restoreJob.isEmpty(); }
    public int encodePercent(int percent) {
        if (percent < 0 || percent > 100) throw new IllegalArgumentException("percent must be 0..100");
        return minimum + Math.round((maximum - minimum) * (percent / 100.0f));
    }

    public static String stableId(String sgbd, String job, String argument) {
        return (sgbd + "_" + job + "_" + argument).toLowerCase(Locale.ROOT)
                .replaceAll("[^a-z0-9]+", "_").replaceAll("^_|_$", "");
    }
}
