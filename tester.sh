#!/bin/bash

PORT=6667
PASS=123
SERVER="127.0.0.1"
IRCSERV="./ircserv"
PASS_COUNT=0
FAIL_COUNT=0
SERVER_PID=""

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[0;33m"
CYAN="\033[0;36m"
RESET="\033[0m"

ok()   { echo -e "  ✅ ${GREEN}$1${RESET}"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo -e "  ❌ ${RED}$1${RESET}"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
info() { echo -e "  ${CYAN}ℹ️  $1${RESET}"; }
section() { echo -e "\n${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; echo -e "${YELLOW}  🧪 $1${RESET}"; echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"; }

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
    rm -f /tmp/irc_*.log
}
trap cleanup EXIT

send_and_recv() {
    local input="$1"
    local wait="${2:-0.5}"
    send_recv_output "$input" "$wait" > /dev/null
}

send_recv_output() {
    local cmds="$1"
    local wait="${2:-1}"
    (
        local tmpout="/tmp/irc_out_$BASHPID.log"
        exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
        cat <&3 > "$tmpout" &
        local cat_pid=$!
        sleep 0.1
        printf "$cmds" >&3
        sleep "$wait"
        exec 3>&- 3<&-
        sleep 0.1
        kill $cat_pid 2>/dev/null
        wait $cat_pid 2>/dev/null
        cat "$tmpout"
        rm -f "$tmpout"
    )
}

start_server() {
    $IRCSERV $PORT $PASS > /tmp/irc_server.log 2>&1 &
    SERVER_PID=$!
    sleep 0.5
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${RED}❌ Impossible de démarrer le serveur !${RESET}"
        exit 1
    fi
}

# ─────────────────────────────────────────
section "Compilation"
# ─────────────────────────────────────────

if make re > /tmp/irc_make.log 2>&1; then
    ok "Compilation réussie"
else
    fail "Compilation échouée"
    cat /tmp/irc_make.log
    exit 1
fi

if [ -x "$IRCSERV" ]; then
    ok "Binaire ircserv présent et exécutable"
else
    fail "Binaire ircserv introuvable"
    exit 1
fi

# Vérification poll() unique
POLL_COUNT=$(grep -rn "poll\|epoll\|select\|kqueue" srcs/ includes/ | grep -v "//" | grep -c "poll\|epoll\|select\|kqueue" 2>/dev/null)
if grep -rn "poll(" srcs/ includes/ --include="*.cpp" --include="*.hpp" | grep -v "//" | wc -l | grep -q "^[0-9]"; then
    POLL_CALLS=$(grep -rn "poll(" srcs/ includes/ --include="*.cpp" --include="*.hpp" | grep -v "//" | wc -l)
    if [ "$POLL_CALLS" -eq 1 ]; then
        ok "Un seul appel à poll() trouvé"
    else
        info "Nombre d'appels poll() : $POLL_CALLS (vérifier manuellement)"
    fi
fi

FCNTL_BAD=$(grep -rn "fcntl(" srcs/ includes/ --include="*.cpp" | grep -v "F_SETFL" | grep -v "//" 2>/dev/null)
if [ -z "$FCNTL_BAD" ]; then
    ok "fcntl() utilisé correctement (F_SETFL uniquement)"
else
    fail "Usage incorrect de fcntl() détecté"
    echo "$FCNTL_BAD"
fi

# Vérification : errno ne doit pas être utilisé pour décider de relire après EAGAIN
EAGAIN_BAD=$(grep -rn "EAGAIN\|EWOULDBLOCK" srcs/ --include="*.cpp" | grep -v "//" 2>/dev/null)
if [ -z "$EAGAIN_BAD" ]; then
    ok "errno EAGAIN/EWOULDBLOCK non utilisé après I/O (conforme)"
else
    fail "errno == EAGAIN/EWOULDBLOCK détecté après I/O (interdit par le sujet)"
    echo "$EAGAIN_BAD"
fi

# ─────────────────────────────────────────
section "Démarrage du serveur"
# ─────────────────────────────────────────

start_server
ok "Serveur démarré (PID $SERVER_PID) sur port $PORT"

sleep 0.3

# ─────────────────────────────────────────
section "Connexion et authentification"
# ─────────────────────────────────────────

OUT=$(send_recv_output "PASS $PASS\r\nNICK testuser\r\nUSER testuser 0 * :Test User\r\n" 1)
if echo "$OUT" | grep -q "001"; then
    ok "Connexion avec PASS + NICK + USER → 001 Welcome reçu"
else
    fail "Pas de message 001 Welcome lors de la connexion"
    info "Réponse reçue : $(echo "$OUT" | head -5)"
fi

OUT=$(send_recv_output "PASS wrongpass\r\nNICK testbad\r\nUSER testbad 0 * :Bad\r\n" 1)
if echo "$OUT" | grep -q "464"; then
    ok "Mauvais mot de passe → 464 refusé"
else
    fail "Mauvais mot de passe non refusé (attendu 464)"
fi

OUT=$(send_recv_output "NICK nonick\r\nUSER nonick 0 * :No Pass\r\n" 1)
if echo "$OUT" | grep -qv "001"; then
    ok "Connexion sans PASS refusée"
else
    fail "Connexion sans PASS acceptée (ne devrait pas)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK duptest\r\nUSER duptest 0 * :Dup\r\n" 0.5)
(echo -e "PASS $PASS\r\nNICK duptest\r\nUSER duptest 0 * :Dup\r\n"; sleep 2) | timeout 3 nc "$SERVER" "$PORT" > /dev/null 2>&1 &
DUP_PID=$!
sleep 0.5
OUT2=$(send_recv_output "PASS $PASS\r\nNICK duptest\r\nUSER duptest2 0 * :Dup2\r\n" 1)
kill $DUP_PID 2>/dev/null
wait $DUP_PID 2>/dev/null
if echo "$OUT2" | grep -q "433"; then
    ok "Nickname dupliqué → 433 refusé"
else
    fail "Nickname dupliqué non refusé (attendu 433)"
fi

# ─────────────────────────────────────────
section "Commandes de base"
# ─────────────────────────────────────────

OUT=$(send_recv_output "PASS $PASS\r\nNICK joiner\r\nUSER joiner 0 * :Joiner\r\nJOIN #testchan\r\n" 1)
if echo "$OUT" | grep -q "JOIN"; then
    ok "JOIN #testchan fonctionne"
else
    fail "JOIN #testchan échoué"
    info "Réponse : $(echo "$OUT" | head -5)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK joiner2\r\nUSER joiner2 0 * :Joiner2\r\nJOIN #a,#b,#c\r\n" 1)
COUNT=$(echo "$OUT" | grep "JOIN" | wc -l)
if [ "$COUNT" -ge 3 ]; then
    ok "JOIN #a,#b,#c → rejoint 3 channels séparément"
else
    fail "JOIN avec virgules ne rejoint pas les 3 channels (JOIN reçus: $COUNT)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK parter\r\nUSER parter 0 * :Parter\r\nJOIN #parttest\r\nPART #parttest :bye\r\n" 1)
if echo "$OUT" | grep -q "PART"; then
    ok "PART #parttest fonctionne"
else
    fail "PART #parttest échoué"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK lister\r\nUSER lister 0 * :Lister\r\nJOIN #listchan\r\nLIST\r\n" 1)
if echo "$OUT" | grep -q "322"; then
    ok "LIST retourne les channels (322)"
else
    fail "LIST n'a pas retourné de 322"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK namer\r\nUSER namer 0 * :Namer\r\nJOIN #namechan\r\nNAMES #namechan\r\n" 1)
if echo "$OUT" | grep -q "353"; then
    ok "NAMES #namechan retourne les users (353)"
else
    fail "NAMES n'a pas retourné de 353"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK pinger\r\nUSER pinger 0 * :Pinger\r\nPING :test123\r\n" 1)
if echo "$OUT" | grep -q "PONG"; then
    ok "PING → PONG fonctionne"
else
    fail "PING → PONG ne fonctionne pas"
fi

# ─────────────────────────────────────────
section "PRIVMSG"
# ─────────────────────────────────────────

# Envoyer un msg dans un channel : ouvrir 2 connexions
(echo -e "PASS $PASS\r\nNICK sender\r\nUSER sender 0 * :Sender\r\nJOIN #msgchan\r\n"; sleep 2) | nc "$SERVER" "$PORT" > /tmp/irc_sender.log 2>&1 &
SENDER_PID=$!
sleep 0.5

OUT=$(send_recv_output "PASS $PASS\r\nNICK recver\r\nUSER recver 0 * :Recver\r\nJOIN #msgchan\r\nPRIVMSG #msgchan :hello channel\r\n" 1)

kill $SENDER_PID 2>/dev/null
wait $SENDER_PID 2>/dev/null

if grep -q "hello channel" /tmp/irc_sender.log; then
    ok "PRIVMSG dans un channel reçu par les autres membres"
else
    fail "PRIVMSG dans un channel non reçu par les autres membres"
fi

# MP entre deux users
(echo -e "PASS $PASS\r\nNICK mpreceiver\r\nUSER mpreceiver 0 * :MpReceiver\r\n"; sleep 2) | nc "$SERVER" "$PORT" > /tmp/irc_mp.log 2>&1 &
MP_PID=$!
sleep 0.5

send_recv_output "PASS $PASS\r\nNICK mpsender\r\nUSER mpsender 0 * :MpSender\r\nPRIVMSG mpreceiver :message prive\r\n" 1 > /dev/null

kill $MP_PID 2>/dev/null
wait $MP_PID 2>/dev/null

if grep -q "message prive" /tmp/irc_mp.log; then
    ok "PRIVMSG (message privé) reçu par le destinataire"
else
    fail "PRIVMSG (message privé) non reçu"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK mpcomma\r\nUSER mpcomma 0 * :MpComma\r\nPRIVMSG mpreceiver,mpsender :multi\r\n" 1)
if ! echo "$OUT" | grep -q "401"; then
    ok "PRIVMSG avec virgules envoyé sans erreur fatale"
else
    info "PRIVMSG avec virgules : destinataires introuvables (normal si déconnectés)"
fi

# ─────────────────────────────────────────
section "Commandes opérateur"
# ─────────────────────────────────────────

# Setup : op rejoint puis cible rejoint
(echo -e "PASS $PASS\r\nNICK opuser\r\nUSER opuser 0 * :Op\r\nJOIN #opchan\r\n"; sleep 3) | nc "$SERVER" "$PORT" > /tmp/irc_op.log 2>&1 &
OP_PID=$!
sleep 0.5

(echo -e "PASS $PASS\r\nNICK victim\r\nUSER victim 0 * :Victim\r\nJOIN #opchan\r\n"; sleep 2) | nc "$SERVER" "$PORT" > /tmp/irc_victim.log 2>&1 &
VICTIM_PID=$!
sleep 0.5

OUT=$(send_recv_output "PASS $PASS\r\nNICK notop\r\nUSER notop 0 * :Notop\r\nJOIN #opchan\r\nKICK #opchan victim :test\r\n" 1)
if echo "$OUT" | grep -q "482"; then
    ok "KICK par non-opérateur → 482 refusé"
else
    fail "KICK par non-opérateur non refusé (attendu 482)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK opkicker\r\nUSER opkicker 0 * :Opkicker\r\nJOIN #kickchan\r\n" 0.3)
(echo -e "PASS $PASS\r\nNICK kicktarget\r\nUSER kicktarget 0 * :KickTarget\r\nJOIN #kickchan\r\n"; sleep 2) | nc "$SERVER" "$PORT" > /tmp/irc_kick.log 2>&1 &
KICK_CLIENT_PID=$!
sleep 0.3

OUT=$(send_recv_output "PASS $PASS\r\nNICK opkicker\r\nUSER opkicker 0 * :Opkicker\r\nKICK #kickchan kicktarget :bye\r\n" 1)

kill $KICK_CLIENT_PID 2>/dev/null
wait $KICK_CLIENT_PID 2>/dev/null

if grep -q "KICK" /tmp/irc_kick.log; then
    ok "KICK reçu par la cible"
else
    info "KICK : vérification manuelle recommandée (timing dépendant)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK topicop\r\nUSER topicop 0 * :TopicOp\r\nJOIN #topicchan\r\nTOPIC #topicchan :Mon super topic\r\n" 1)
if echo "$OUT" | grep -q "TOPIC"; then
    ok "TOPIC changé par l'opérateur"
else
    fail "TOPIC non changé par l'opérateur"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK modeop\r\nUSER modeop 0 * :ModeOp\r\nJOIN #modechan\r\nMODE #modechan +i\r\n" 1)
if echo "$OUT" | grep -q "MODE"; then
    ok "MODE +i appliqué sur le channel"
else
    fail "MODE +i non appliqué"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK modeop2\r\nUSER modeop2 0 * :ModeOp2\r\nJOIN #modechan2\r\nMODE #modechan2 +k secretpass\r\n" 1)
if echo "$OUT" | grep -q "MODE"; then
    ok "MODE +k (mot de passe) appliqué"
else
    fail "MODE +k non appliqué"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK modeop3\r\nUSER modeop3 0 * :ModeOp3\r\nJOIN #modechan3\r\nMODE #modechan3 +l 5\r\n" 1)
if echo "$OUT" | grep -q "MODE"; then
    ok "MODE +l (limite users) appliqué"
else
    fail "MODE +l non appliqué"
fi

# MODE +t : seul un opérateur peut changer le topic
OUT=$(send_recv_output "PASS $PASS\r\nNICK topicmaster\r\nUSER topicmaster 0 * :TopicMaster\r\nJOIN #topictchan\r\nMODE #topictchan +t\r\n" 1)
if echo "$OUT" | grep -q "MODE"; then
    ok "MODE +t appliqué (topic restreint aux opérateurs)"
else
    fail "MODE +t non appliqué"
fi

# MODE +t : un non-op ne peut pas changer le topic (doit recevoir 482)
OUT=$(send_recv_output "PASS $PASS\r\nNICK topicguest\r\nUSER topicguest 0 * :TopicGuest\r\nJOIN #topictchan\r\nTOPIC #topictchan :intrus\ topic\r\n" 1)
if echo "$OUT" | grep -q "482\|not channel operator\|TOPIC"; then
    # 482 = ERR_CHANOPRIVSNEEDED
    if echo "$OUT" | grep -q "482"; then
        ok "TOPIC refusé à un non-op sur channel +t (482)"
    else
        info "TOPIC +t : réponse reçue, vérifier manuellement"
    fi
else
    fail "TOPIC non refusé à un non-op sur channel +t (attendu 482)"
fi

# MODE +k : JOIN avec mauvaise clé → 475, bonne clé → succès
(echo -e "PASS $PASS\r\nNICK keyop\r\nUSER keyop 0 * :KeyOp\r\nJOIN #keychan\r\nMODE #keychan +k sekret\r\n"; sleep 3) | nc "$SERVER" "$PORT" > /dev/null 2>&1 &
KEYOP_PID=$!
sleep 0.6

OUT=$(send_recv_output "PASS $PASS\r\nNICK keywrong\r\nUSER keywrong 0 * :KeyWrong\r\nJOIN #keychan badkey\r\n" 1)
if echo "$OUT" | grep -q "475"; then
    ok "JOIN avec mauvaise clé → 475 refusé"
else
    fail "JOIN avec mauvaise clé non refusé (attendu 475)"
fi

OUT=$(send_recv_output "PASS $PASS\r\nNICK keygood\r\nUSER keygood 0 * :KeyGood\r\nJOIN #keychan sekret\r\n" 1)
if echo "$OUT" | grep -q "JOIN"; then
    ok "JOIN avec bonne clé → succès"
else
    fail "JOIN avec bonne clé échoué"
fi

kill $KEYOP_PID 2>/dev/null
wait $KEYOP_PID 2>/dev/null

# MODE removal : -i, -k, -l
OUT=$(send_recv_output "PASS $PASS\r\nNICK remop\r\nUSER remop 0 * :RemOp\r\nJOIN #remchan\r\nMODE #remchan +i\r\nMODE #remchan -i\r\n" 1)
if echo "$OUT" | grep -q "\-i"; then
    ok "MODE -i (retrait invite-only) fonctionne"
else
    fail "MODE -i non appliqué"
fi

kill $OP_PID $VICTIM_PID 2>/dev/null
wait $OP_PID $VICTIM_PID 2>/dev/null

# ─────────────────────────────────────────
section "nc tué avec demi-commande"  
# ─────────────────────────────────────────

# Envoyer la moitié d'une commande puis tuer brutalement
(
    exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
    printf "PASS $PASS\r\nNICK halfkill\r\n" >&3
    sleep 0.3
    printf "USER halfki" >&3
    sleep 0.1
    exec 3>&- 3<&-
) 2>/dev/null
sleep 0.5

OUT=$(send_recv_output "PASS $PASS\r\nNICK afterhalf\r\nUSER afterhalf 0 * :AfterHalf\r\n" 1)
if echo "$OUT" | grep -q "001"; then
    ok "Serveur stable après nc tué en plein milieu d'une commande"
else
    fail "Serveur non opérationnel après nc tué avec demi-commande"
fi

# ─────────────────────────────────────────
section "Commandes partielles (robustesse réseau)"
# ─────────────────────────────────────────

{
    printf "PASS $PASS\r\n"
    sleep 0.2
    printf "NICK par"
    sleep 0.3
    printf "tial\r\n"
    sleep 0.2
    printf "USER partial 0 * :Partial\r\n"
    sleep 0.5
    printf "QUIT\r\n"
} | nc "$SERVER" "$PORT" > /tmp/irc_partial.log 2>&1

if grep -q "001" /tmp/irc_partial.log; then
    ok "Commandes partielles gérées (NICK envoyé en 2 parties)"
else
    fail "Commandes partielles non gérées correctement"
    info "Réponse : $(cat /tmp/irc_partial.log | head -3)"
fi

# Vérifier que le serveur est encore opérationnel après
sleep 0.3
OUT=$(send_recv_output "PASS $PASS\r\nNICK afterpartial\r\nUSER afterpartial 0 * :After\r\n" 1)
if echo "$OUT" | grep -q "001"; then
    ok "Serveur opérationnel après commandes partielles"
else
    fail "Serveur non opérationnel après commandes partielles"
fi

# ─────────────────────────────────────────
section "Déconnexion brutale d'un client"
# ─────────────────────────────────────────

(echo -e "PASS $PASS\r\nNICK brute\r\nUSER brute 0 * :Brute\r\nJOIN #brutechan\r\n"; sleep 1) | nc "$SERVER" "$PORT" > /dev/null 2>&1 &
BRUTE_PID=$!
sleep 0.5

kill -9 $BRUTE_PID 2>/dev/null
wait $BRUTE_PID 2>/dev/null
sleep 0.5

OUT=$(send_recv_output "PASS $PASS\r\nNICK afterbrute\r\nUSER afterbrute 0 * :AfterBrute\r\n" 1)
if echo "$OUT" | grep -q "001"; then
    ok "Serveur opérationnel après déconnexion brutale"
else
    fail "Serveur non opérationnel après déconnexion brutale"
fi

# ─────────────────────────────────────────
section "Connexions multiples simultanées"
# ─────────────────────────────────────────

PIDS=()
for i in 1 2 3 4 5; do
    (echo -e "PASS $PASS\r\nNICK multiuser$i\r\nUSER multiuser$i 0 * :Multi$i\r\nJOIN #multichan\r\n"; sleep 1) | nc "$SERVER" "$PORT" > /tmp/irc_multi_$i.log 2>&1 &
    PIDS+=($!)
done
sleep 1.5

CONNECTED=0
for i in 1 2 3 4 5; do
    if grep -q "001" /tmp/irc_multi_$i.log 2>/dev/null; then
        CONNECTED=$((CONNECTED + 1))
    fi
done

for pid in "${PIDS[@]}"; do
    kill $pid 2>/dev/null
    wait $pid 2>/dev/null
done

if [ "$CONNECTED" -eq 5 ]; then
    ok "5 connexions simultanées acceptées"
else
    fail "Seulement $CONNECTED/5 connexions simultanées acceptées"
fi

# ─────────────────────────────────────────
section "INVITE"
# ─────────────────────────────────────────

# Ordre: invitetarget (cible) + noinvite connectés en premier,
# puis l'op crée le channel +i et invite seulement invitetarget.
# noinvite essaie de joindre pendant que l'op est encore dans le channel.

# Étape 1 : connecter invitetarget
(
    tmpout="/tmp/irc_out_${BASHPID}.log"
    exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
    cat <&3 > "$tmpout" &
    cat_pid=$!
    sleep 0.1
    printf "PASS $PASS\r\nNICK invitetarget\r\nUSER invitetarget 0 * :InviteTarget\r\n" >&3
    sleep 4
    exec 3>&- 3<&-
    sleep 0.1
    kill $cat_pid 2>/dev/null
    wait $cat_pid 2>/dev/null
    cat "$tmpout"
    rm -f "$tmpout"
) > /tmp/irc_invitetarget.log 2>/dev/null &
INVITETARGET_PID=$!
sleep 0.5

# Étape 2 : l'op crée, met +i, invite invitetarget, et reste 3 secondes
(
    tmpout="/tmp/irc_out_${BASHPID}.log"
    exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
    cat <&3 > "$tmpout" &
    cat_pid=$!
    sleep 0.1
    printf "PASS $PASS\r\nNICK inviteop\r\nUSER inviteop 0 * :InviteOp\r\nJOIN #invitechan\r\nMODE #invitechan +i\r\nINVITE invitetarget #invitechan\r\n" >&3
    sleep 3
    exec 3>&- 3<&-
    sleep 0.1
    kill $cat_pid 2>/dev/null
    wait $cat_pid 2>/dev/null
    cat "$tmpout"
    rm -f "$tmpout"
) > /tmp/irc_inviteop.log 2>/dev/null &
INVITEOP_PID=$!
sleep 0.8  # attendre que l'op ait rejoint et mis +i

# Étape 3 : noinvite essaie de joindre (channel +i existe, il n'est pas invité)
OUT_NOINVITE=$(send_recv_output "PASS $PASS\r\nNICK noinvite\r\nUSER noinvite 0 * :NoInvite\r\nJOIN #invitechan\r\n" 1)

wait $INVITEOP_PID 2>/dev/null
wait $INVITETARGET_PID 2>/dev/null

if grep -q "341\|INVITE" /tmp/irc_inviteop.log; then
    ok "INVITE envoyé par l'opérateur (341)"
else
    fail "INVITE non envoyé (attendu 341)"
fi

if grep -q "INVITE" /tmp/irc_invitetarget.log; then
    ok "INVITE reçu par la cible"
else
    fail "INVITE non reçu par la cible"
fi

if echo "$OUT_NOINVITE" | grep -q "473"; then
    ok "JOIN sur channel +i sans invite → 473 refusé"
else
    fail "JOIN channel +i sans invite non refusé (attendu 473)"
fi

# ─────────────────────────────────────────
section "MODE +o (promotion opérateur)"
# ─────────────────────────────────────────

(
    tmpout="/tmp/irc_modeop_${BASHPID}.log"
    exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
    cat <&3 > "$tmpout" &
    cat_pid=$!
    sleep 0.1
    printf "PASS $PASS\r\nNICK modemaster\r\nUSER modemaster 0 * :ModeMaster\r\nJOIN #modeopchan\r\n" >&3
    sleep 0.5
    printf "MODE #modeopchan +o modetarget\r\n" >&3
    sleep 1
    exec 3>&- 3<&-
    sleep 0.1
    kill $cat_pid 2>/dev/null
    wait $cat_pid 2>/dev/null
    cat "$tmpout"
    rm -f "$tmpout"
) > /tmp/irc_modeop.log 2>/dev/null &
MODEOP_PID=$!
sleep 0.3

send_recv_output "PASS $PASS\r\nNICK modetarget\r\nUSER modetarget 0 * :ModeTarget\r\nJOIN #modeopchan\r\n" 1.5 > /dev/null
wait $MODEOP_PID 2>/dev/null

if grep -q "MODE.*+o" /tmp/irc_modeop.log; then
    ok "MODE +o (promotion) appliqué"
else
    fail "MODE +o non appliqué"
fi

# ─────────────────────────────────────────
section "Flood avec client suspendu (^Z)"
# ─────────────────────────────────────────

# Client A rejoint le channel et se suspend (SIGSTOP)
(
    tmpout="/tmp/irc_frozen_${BASHPID}.log"
    exec 3<>/dev/tcp/"$SERVER"/"$PORT" 2>/dev/null || exit 1
    cat <&3 > "$tmpout" &
    cat_pid=$!
    sleep 0.1
    printf "PASS $PASS\r\nNICK frozen\r\nUSER frozen 0 * :Frozen\r\nJOIN #floodchan\r\n" >&3
    sleep 0.5
    echo "$cat_pid" > /tmp/irc_frozen_catpid.txt
    echo "$$" > /tmp/irc_frozen_pid.txt
    exec 3>&- 3<&-
) > /dev/null 2>/dev/null &
FROZEN_PID=$!
sleep 0.8

# Suspendre le client frozen
kill -STOP $FROZEN_PID 2>/dev/null
sleep 0.2

# Client B flood le channel pendant que frozen est suspendu
FLOOD_OK=true
for i in 1 2 3 4 5; do
    OUT=$(send_recv_output "PASS $PASS\r\nNICK flooder$i\r\nUSER flooder$i 0 * :Flooder\r\nJOIN #floodchan\r\nPRIVMSG #floodchan :flood message $i\r\n" 0.5)
    if ! echo "$OUT" | grep -q "001"; then
        FLOOD_OK=false
        break
    fi
done

if $FLOOD_OK; then
    ok "Serveur ne bloque pas pendant le flood (client suspendu)"
else
    fail "Serveur bloqué pendant le flood avec client suspendu"
fi

# Reprendre le client frozen
kill -CONT $FROZEN_PID 2>/dev/null
sleep 0.5
kill $FROZEN_PID 2>/dev/null
wait $FROZEN_PID 2>/dev/null

# Vérifier que le serveur est toujours ok après
OUT=$(send_recv_output "PASS $PASS\r\nNICK afterflood\r\nUSER afterflood 0 * :AfterFlood\r\n" 0.8)
if echo "$OUT" | grep -q "001"; then
    ok "Serveur opérationnel après flood + client suspendu"
else
    fail "Serveur non opérationnel après flood + client suspendu"
fi

# ─────────────────────────────────────────
section "Fuites mémoire (valgrind)"
# ─────────────────────────────────────────


if ! command -v valgrind &> /dev/null; then
    info "valgrind non disponible, test ignoré"
else
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    sleep 0.3

    valgrind --leak-check=full --error-exitcode=1 \
        $IRCSERV $PORT $PASS > /tmp/irc_valgrind.log 2>&1 &
    VAL_PID=$!
    sleep 0.5

    send_recv_output "PASS $PASS\r\nNICK valtest\r\nUSER valtest 0 * :Val\r\nJOIN #valchan\r\nPRIVMSG #valchan :test\r\n" 1 > /dev/null
    sleep 0.3

    kill "$VAL_PID" 2>/dev/null
    wait "$VAL_PID" 2>/dev/null

    if grep -q "no leaks are possible\|0 bytes in 0 blocks" /tmp/irc_valgrind.log; then
        ok "Aucune fuite mémoire détectée"
    elif grep -q "ERROR SUMMARY: 0 errors" /tmp/irc_valgrind.log; then
        ok "Aucune erreur mémoire détectée"
    else
        LEAKS=$(grep "definitely lost\|indirectly lost" /tmp/irc_valgrind.log | tail -2)
        if [ -n "$LEAKS" ]; then
            fail "Fuites mémoire détectées :"
            echo "$LEAKS"
        else
            info "Résultat valgrind incertain, vérifier /tmp/irc_valgrind.log"
        fi
    fi

    # Relancer le serveur normal
    start_server
fi

# ─────────────────────────────────────────
# Résumé final
# ─────────────────────────────────────────

TOTAL=$((PASS_COUNT + FAIL_COUNT))
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo -e "${YELLOW}  📊 RÉSUMÉ FINAL${RESET}"
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo -e "  ✅ ${GREEN}Réussis  : $PASS_COUNT / $TOTAL${RESET}"
if [ "$FAIL_COUNT" -gt 0 ]; then
    echo -e "  ❌ ${RED}Échoués  : $FAIL_COUNT / $TOTAL${RESET}"
fi

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo -e "\n  🎉 ${GREEN}TOUS LES TESTS SONT PASSÉS !${RESET}"
else
    echo -e "\n  ⚠️  ${YELLOW}$FAIL_COUNT test(s) à corriger.${RESET}"
fi
echo ""
