package main

import (
	"encoding/binary"
	"encoding/csv"
	"flag"
	"fmt"
	"log"
	"math"
	"net"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/Nekhaevalex/nphauto/sensors/moisture"
)

type params struct {
	path     string
	interval time.Duration
	quantity time.Duration
}

var p params

func init() {
	// Parsing arguments
	tablePath := flag.String("table", "data.csv", "path to table csv file")
	frequency := flag.Int("int", 12, "tick time interval numeric part")
	sQuanity := flag.String("qty", "h", "tick time interval quantity (h/m/s)")
	flag.Parse()

	var quantity time.Duration
	switch strings.TrimSpace(*sQuanity) {
	case "h":
		quantity = time.Hour
	case "m":
		quantity = time.Minute
	case "s":
		quantity = time.Second
	default:
		log.Fatalf("wrong quantity '%s', expected h/m/s", *sQuanity)
	}

	p = params{
		path:     *tablePath,
		interval: time.Duration(*frequency),
		quantity: quantity,
	}
}

func setupOnDemandListener(ch chan float64) {
	socket, err := net.Listen("unix", "/tmp/moisture.sock")
	if err != nil {
		log.Fatal(err)
	}

	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-c
		os.Remove("/tmp/moisture.sock")
		os.Exit(1)
	}()

	for {
		conn, err := socket.Accept()
		if err != nil {
			log.Fatal(err)
		}

		go func(conn net.Conn) {
			defer conn.Close()

			buff := make([]byte, 128)
			if _, err := conn.Read(buff); err != nil {
				log.Fatal(err)
			}
			ch <- 1.0
			reply := <-ch

			var bf []byte

			binary.LittleEndian.PutUint64(bf, math.Float64bits(reply))
			if _, err := conn.Write(bf); err != nil {
				log.Fatal(err)
			}
		}(conn)
	}

}

func main() {
	// Setting up thread ticker
	ticker := time.NewTicker(p.interval * p.quantity)
	// Thread done channel
	done := make(chan bool)

	// Setting up moisture sensor connection
	ms, err := moisture.NewMoistureSensor("DracaenaMoisture")
	if err != nil {
		log.Fatal(err)
	}

	// Connecting
	if err = ms.Connect(); err != nil {
		log.Fatal(err)
	}

	defer func() {
		if err := ms.Disconnect(); err != nil {
			log.Fatal(err)
		}
	}()

	// Setting up CSV file
	file, err := os.OpenFile(p.path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0660)
	if err != nil {
		log.Fatal(err)
	}

	defer func() {
		if err := file.Close(); err != nil {
			log.Fatal(err)
		}
	}()

	var data float64              // Variable for storing moisture value
	writer := csv.NewWriter(file) // Connecting writer to CSV file

	defer func() {
		writer.Flush()
		if err := writer.Error(); err != nil {
			log.Fatal(err)
		}
	}()

	// Start on-demand value process
	onDemand := make(chan float64, 1)
	go setupOnDemandListener(onDemand)

	// Reading loop
	for {
		select {
		case <-ticker.C:
			// On tick
			err = ms.Read(&data)
			if err != nil {
				log.Fatal(err)
			}

			now := fmt.Sprintf("%d", time.Now().Unix())
			val := fmt.Sprintf("%f", data)

			fmt.Printf("%s: %f\n", time.Now().String(), data)

			if err = writer.Write([]string{now, val}); err != nil {
				log.Fatal(err)
			}

			writer.Flush()
			if err = writer.Error(); err != nil {
				log.Fatal(err)
			}
		case <-onDemand:
			err = ms.Read(&data)
			if err != nil {
				log.Fatal(err)
			}

			now := fmt.Sprintf("%d", time.Now().Unix())
			val := fmt.Sprintf("%f", data)

			fmt.Printf("%s: %f (on demand)\n", time.Now().String(), data)

			if err = writer.Write([]string{now, val}); err != nil {
				log.Fatal(err)
			}

			writer.Flush()
			if err = writer.Error(); err != nil {
				log.Fatal(err)
			}

			onDemand <- data
		case <-done:
			// On finish
			return
		}
	}
}
