% Root locus of the permeate flow PD loop: target r (pulses/min), flow y (pulses/min),
% controller output u = Kp(r - y) - Kd*y' (RO pump speed units/s):
%
%   Y(s)/R(s) = a*Kp / (tau*s^2 + (1 + a*Kd)*s + a*Kp)
%
% Left: sweeps Kp with Kd fixed. Right: sweeps Kd with Kp fixed.
% Red squares: the closed-loop poles at the Kp and Kd below.

a = 1956;      % pulses/min per unit speed, from the speed ramp regression
tau = 5;       % s, PLACEHOLDER: measure it with a speed step
Kp = 5.2e-5;   % (speed/s) per (pulses/min)
Kd = 2e-5;     % (speed/s) per (pulses/min)/s

% 1 + Kp*L(s) = 0 and 1 + Kd*L(s) = 0, from the denominator above
L_Kp = tf(a, [tau, 1 + a*Kd, 0]);
L_Kd = tf([a, 0], [tau, 1, a*Kp]);

closed_loop = tf(a*Kp, [tau, 1 + a*Kd, a*Kp]);
poles = pole(closed_loop);

figure('Name', 'Flow PD root locus', 'NumberTitle', 'off', 'Position', [50 50 1200 520]);

subplot(1, 2, 1);
rlocus(L_Kp);
hold on;
plot(real(poles), imag(poles), 'rs', 'MarkerFaceColor', 'r');
hold off;
sgrid;
title(sprintf('Sweep Kp, Kd = %.3g', Kd));

subplot(1, 2, 2);
rlocus(L_Kd);
hold on;
plot(real(poles), imag(poles), 'rs', 'MarkerFaceColor', 'r');
hold off;
sgrid;
title(sprintf('Sweep Kd, Kp = %.3g', Kp));

[wn, zeta] = damp(closed_loop);
fprintf('Closed-loop poles at Kp = %.3g, Kd = %.3g:\n', Kp, Kd);
for i = 1:numel(poles)
    fprintf('  %.4f %+.4fj  (wn = %.4f rad/s, damping = %.3f)\n', ...
            real(poles(i)), imag(poles(i)), wn(i), zeta(i));
end
