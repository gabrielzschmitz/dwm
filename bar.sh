#!/bin/sh

# dwm status bar for NixOS
# ^c$var^ = foreground color
# ^b$var^ = background color

set -u

# One Dark colors
black='#14171c'
white='#abb2bf'
grey='#282c34'
blue='#7aa2f7'
cyan='#56b6c2'
green='#7eca9c'
red='#d47d85'
orange='#caaa6a'
yellow='#EBCB8B'
pink='#c678dd'
darkblue='#668ee3'

pomodoro_file="${XDG_RUNTIME_DIR:-/tmp}/dwm-pomodoro"

(
  while true; do
    stdbuf -oL tomato -t 2>/dev/null | while IFS= read -r line; do
      printf '%s\n' "$line" > "$pomodoro_file"
    done
    sleep 1
  done
) &
trap 'kill 0' EXIT

pomodoro() {
  pomodoro_output=$(find "$pomodoro_file" -newermt '-3 seconds' 2>/dev/null | grep -q . && tail -n 1 "$pomodoro_file" 2>/dev/null | tr -d '\r' | sed 's/^[[:space:]]*//') || return
  [ -n "$pomodoro_output" ] || return
  printf '^c%s^^b%s^ %s ^b%s^' "$black" "$red" "$pomodoro_output" "$black"
}

cpu_file="${XDG_RUNTIME_DIR:-/tmp}/dwm-cpu"
cpu() {
  read -r cpu_label cpu_user cpu_nice cpu_system cpu_idle cpu_iowait cpu_irq cpu_softirq cpu_steal cpu_rest < /proc/stat
  cpu_total=$((cpu_user + cpu_nice + cpu_system + cpu_idle + cpu_iowait + cpu_irq + cpu_softirq + cpu_steal))
  cpu_busy=$((cpu_total - cpu_idle - cpu_iowait))
  cpu_prev_total=0
  cpu_prev_busy=0
  cpu_history=

  if [ -r "$cpu_file" ]; then
    read -r cpu_prev_total cpu_prev_busy cpu_history < "$cpu_file" || :
  fi

  cpu_percentage=-1
  if [ "$cpu_prev_total" -gt 0 ] && [ "$cpu_total" -gt "$cpu_prev_total" ]; then
    cpu_percentage=$(((cpu_busy - cpu_prev_busy) * 100 / (cpu_total - cpu_prev_total)))
  fi

  set -- $cpu_history
  if [ "$cpu_percentage" -ge 0 ]; then
    set -- "$@" "$cpu_percentage"
  fi
  while [ "$#" -gt 5 ]; do
    shift
  done
  cpu_history=$*
  printf '%s %s %s\n' "$cpu_total" "$cpu_busy" "$cpu_history" > "$cpu_file"

  cpu_count=$#
  if [ "$cpu_count" -gt 0 ]; then
    cpu_percentage=$(printf '%s\n' "$@" | sort -n | awk -v n="$cpu_count" '
      { a[NR] = $1 }
      END { print (n % 2) ? a[(n + 1) / 2] : int((a[n / 2] + a[n / 2 + 1]) / 2) }')
  else
    cpu_percentage=0
  fi

  printf '^c%s^^b%s^  ' "$black" "$cyan"
  printf '^c%s^^b%s^ %s%% ^b%s^' "$white" "$grey" "$cpu_percentage" "$black"
}

brightness() {
  for brightness_device in /sys/class/backlight/*; do
    [ -d "$brightness_device" ] || continue
    brightness_current=$(cat "$brightness_device/brightness" 2>/dev/null) || continue
    brightness_maximum=$(cat "$brightness_device/max_brightness" 2>/dev/null) || continue
    [ "$brightness_maximum" -gt 0 ] || continue
    brightness_percentage=$((brightness_current * 100 / brightness_maximum))
    printf '^c%s^ %s%%' "$red" "$brightness_percentage"
    return
  done
}

mem() {
  mem_used=$(awk '
  /^MemTotal:/ { total = $2 }
  /^MemAvailable:/ { available = $2 }
  END {
    if (total > 0)
      printf "%.1fG", (total - available) / 1048576
  }
  ' /proc/meminfo)

  printf '^c%s^^b%s^ 󰘚 ' "$black" "$pink"
  printf '^c%s^^b%s^ %s ^b%s^' "$white" "$grey" "${mem_used:-N/A}" "$black"
}

volume() {
  volume_output=$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null) || return

  case "$volume_output" in
    *'[MUTED]'*)
      volume_icon=''
      volume_percentage=0
      ;;
    *)
      volume_value=$(printf '%s\n' "$volume_output" | sed -n 's/.*Volume: \([0-9.][0-9.]*\).*/\1/p')
      [ -n "$volume_value" ] || return
      volume_percentage=$(awk -v v="$volume_value" 'BEGIN { printf "%.0f", v * 100 }')
      if [ "$volume_percentage" -le 33 ]; then
        volume_icon='󰕿'
      elif [ "$volume_percentage" -le 66 ]; then
        volume_icon='󰖀'
      else
        volume_icon='󰕾'
      fi
      ;;
  esac

  printf '^c%s^^b%s^ %s ' "$black" "$orange" "$volume_icon"
  printf '^c%s^^b%s^ %s%%^b%s^' "$orange" "$black" "$volume_percentage" "$black"
}

battery() {
  for battery_dir in /sys/class/power_supply/BAT*; do
    [ -d "$battery_dir" ] || continue
    battery_capacity=$(cat "$battery_dir/capacity" 2>/dev/null) || continue
    battery_status=$(cat "$battery_dir/status" 2>/dev/null || true)

    printf '^c%s^^b%s^ 󱊣 ' "$black" "$green"
    printf '^c%s^^b%s^ %s%%' "$green" "$black" "$battery_capacity"
    [ "$battery_status" = 'Charging' ] && printf '^c%s^+' "$green"
    printf '^b%s^' "$black"
    return
  done
}

network() {
  # Ethernet takes priority when connected.
  for network_device in /sys/class/net/enp*; do
    [ -d "$network_device" ] || continue
    network_state=$(cat "$network_device/operstate" 2>/dev/null || true)
    if [ "$network_state" = 'up' ]; then
      printf '^c%s^^b%s^ 󰈁 ^d^%s' "$black" "$blue" ' ^c#7aa2f7^Connected'
      return
    fi
  done

  # Otherwise, show Wi-Fi, whether connected or disconnected.
  for network_device in /sys/class/net/*; do
    [ -d "$network_device/wireless" ] || continue
    network_state=$(cat "$network_device/operstate" 2>/dev/null || true)
    if [ "$network_state" = 'up' ]; then
      printf '^c%s^^b%s^ 󰤨 ^d^%s' "$black" "$blue" ' ^c#7aa2f7^Connected'
    else
      printf '^c%s^^b%s^ 󰤭 ^d^%s' "$black" "$blue" ' ^c#7aa2f7^Disconnected'
    fi
    return
  done
}

clock() {
  printf '^c%s^^b%s^ 󱑆 ' "$black" "$darkblue"
  printf '^c%s^^b%s^ %s ' "$black" "$blue" "$(date '+%H:%M')"
}

while true; do
  status_lines=$(
    for module in pomodoro volume battery cpu mem clock; do
      "$module"
      printf '\n'
    done
  )
  status=
  while IFS= read -r status_output; do
    [ -n "$status_output" ] || continue
    [ -n "$status" ] && status="$status "
    status="$status$status_output"
  done <<EOF
$status_lines
EOF

  xsetroot -name " $status" 2>/dev/null || exit 1
  sleep 0.25
done
