package com.synaro.bmwe9xcontrol.frm;

import com.synaro.bmwe9xcontrol.ediabas.EdiabasArgument;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasJobDefinition;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class FrmCapabilityDiscovery {
    private FrmCapabilityDiscovery() {}

    /**
     * Produces candidates only from real SGBD metadata. A candidate is deliberately
     * unarmed until reviewed on the target vehicle.
     */
    public static List<FrmCapability> fromJobs(String sgbd, List<EdiabasJobDefinition> jobs) {
        List<FrmCapability> result = new ArrayList<>();
        for (EdiabasJobDefinition job : jobs) {
            String searchable = (job.name + " " + job.description).toUpperCase(Locale.ROOT);
            if (!(searchable.contains("STEUERN_") || searchable.contains("LAMPE")
                    || searchable.contains("PWM") || searchable.contains("AUSGANG"))) continue;
            for (EdiabasArgument argument : job.arguments) {
                if (argument.name.trim().isEmpty()) continue;
                double rawMin = argument.minimum == null ? 0 : argument.minimum;
                double rawMax = argument.maximum == null ? inferMaximum(argument) : argument.maximum;
                if (rawMax < rawMin || rawMax > Integer.MAX_VALUE || rawMin < Integer.MIN_VALUE) continue;
                int min = (int) Math.round(rawMin);
                int max = (int) Math.round(rawMax);
                String argText = (argument.name + " " + argument.description).toUpperCase(Locale.ROOT);
                boolean pwm = max > 1 || argText.contains("PWM") || argText.contains("PROZENT");
                boolean onOff = max >= 1;
                if (!onOff && !pwm) continue;
                String evidence = "DOCUMENTED_SGBD_METADATA: " + job.name + "/" + argument.name;
                result.add(new FrmCapability(FrmCapability.stableId(sgbd, job.name, argument.name),
                        job.description.trim().isEmpty() ? job.name + " / " + argument.name
                                : job.description + " / " + argument.name,
                        sgbd, job.name, argument.name, onOff, pwm, min, max, evidence));
            }
        }
        return result;
    }

    private static double inferMaximum(EdiabasArgument argument) {
        if (!argument.documentedValues.isEmpty()) return argument.documentedValues.size() - 1;
        String text = (argument.name + " " + argument.description).toUpperCase(Locale.ROOT);
        return text.contains("PWM") || text.contains("PROZENT") ? 100 : 1;
    }
}
