% Plots every field of a ro_pump_speed_test CSV against time. Run from anywhere;
% the CSV is looked up next to this script. Point csv_name at another run to plot it.

csv_name = 'ro_pump_speed_test_20261105_030309.csv';

data = readtable(fullfile(fileparts(mfilename('fullpath')), csv_name), 'TextType', 'string');
t = data.time_s;

% One panel per entry: title, y label, columns
panels = {
    'RO pump speed',           'Command (0-1)', {'ro_speed_command', 'settling'}
    'Media and Aqualine',      'Pressure (psi)', {'media_inlet_pressure_psi', 'media_outlet_pressure_psi', ...
                                                  'aqualine_inlet_pressure_psi', 'aqualine_outlet_pressure_psi'}
    'RO membranes',            'Pressure (psi)', {'primary_pressure_psi', 'interstage_pressure_psi', ...
                                                  'concentrate_pressure_psi', 'permeate_pressure_psi'}
    'CIP and air',             'Pressure (psi)', {'cip_inlet_pressure_psi', 'cip_outlet_pressure_psi', 'air_pressure_psi'}
    'Flow',                    'Flow (gpm)',     {'feed_flow_gpm', 'permeate_flow_gpm', 'concentrate_flow_gpm'}
    'Feed conductivity',       'uS/cm',          {'feed_conductivity_us_cm'}
    'Permeate conductivity',   'uS/cm',          {'permeate_conductivity_us_cm'}
};

figure('Name', csv_name, 'NumberTitle', 'off', 'Position', [50 50 1300 900]);
layout = tiledlayout(4, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
title(layout, sprintf('RO pump speed ramp, RO state: %s', strjoin(unique(data.ro_state), ', ')), ...
      'Interpreter', 'none');

for i = 1:size(panels, 1)
    nexttile;
    hold on;
    columns = panels{i, 3};
    for j = 1:numel(columns)
        plot(t, data.(columns{j}), 'LineWidth', 1.2, 'DisplayName', columns{j});
    end
    hold off;
    grid on;
    box on;
    xlim([t(1) t(end)]);
    title(panels{i, 1});
    xlabel('Time (s)');
    ylabel(panels{i, 2});
    if numel(columns) > 1
        legend('Location', 'eastoutside', 'Interpreter', 'none');
    end
end

% Permeate / concentrate flow. Skipped below MIN_CONCENTRATE_GPM (the first rows):
% dividing by almost no flow only gives spikes
MIN_CONCENTRATE_GPM = 5;
ratio = data.permeate_flow_gpm ./ data.concentrate_flow_gpm;
ratio(~(data.concentrate_flow_gpm >= MIN_CONCENTRATE_GPM)) = NaN;

nexttile;
plot(t, ratio, 'LineWidth', 1.2);
grid on;
box on;
xlim([t(1) t(end)]);
title('Permeate flow / concentrate flow');
xlabel('Time (s)');
ylabel('Ratio');
