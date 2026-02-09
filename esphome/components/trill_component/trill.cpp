/*
 * Trill library for ESPHome
 * Adapted from Arduino version (c) 2020 bela.io
 *
 * This library communicates with the Trill sensors
 * using I2C via ESPHome's I2C component.
 *
 * BSD license
 */

#include "trill.h"

#define MAX_TOUCH_1D_OR_2D (((device_type_ == TRILL_SQUARE || device_type_ == TRILL_HEX) ? kMaxTouchNum2D : kMaxTouchNum1D))
#define RAW_LENGTH ((device_type_ == TRILL_BAR ? 2 * kNumChannelsBar \
			: device_type_ == TRILL_RING ? 2 * kNumChannelsRing \
			: 2 * kNumChannelsMax))

Trill::Trill()
	: i2c_device_(nullptr), device_type_(TRILL_NONE), mode_(AUTO),
	firmware_version_(0), last_read_loc_(0xFF), raw_bytes_left_(0),
	rx_buffer_index_(0), rx_buffer_length_(0)
{
}

/* Initialise the hardware. Returns the type of device attached, or 0
   if none is attached. */
int Trill::begin(Device device, esphome::i2c::I2CDevice* i2c_device) {
	i2c_device_ = i2c_device;

	if (i2c_device_ == nullptr) {
		return -2;
	}

	/* Check the type of device attached */
	if (identify() != 0) {
		// Unable to identify device
		return 2;
	}

	/* Check for wrong device type */
	if (TRILL_UNKNOWN != device && device_type_ != device) {
		device_type_ = TRILL_NONE;
		return -3;
	}

	/* Check for device mode */
	Mode mode = trillDefaults[device + 1].mode;
	if (AUTO == mode) {
		return -1;
	}

	/* Put the device in the correspondent mode */
	setMode(mode);
	esphome::delay(interCommandDelay);

	Touches::centroids = buffer_;
	Touches::sizes = buffer_ + MAX_TOUCH_1D_OR_2D;
	if (is2D()) {
		horizontal.centroids = buffer_ + 2 * MAX_TOUCH_1D_OR_2D;
		horizontal.sizes = buffer_ + 3 * MAX_TOUCH_1D_OR_2D;
	}
	else
		horizontal.num_touches = 0;

	/* Set default scan settings */
	setScanSettings(0, 12);
	esphome::delay(interCommandDelay);

	updateBaseline();
	esphome::delay((firmware_version_ >= 3 ? 10 : 1) * interCommandDelay);

	return 0;
}

/* Return the type of device attached, or 0 if none is attached. */
int Trill::identify() {
	uint8_t cmd[2] = { kOffsetCommand, kCommandIdentify };

	if (i2c_device_->write(cmd, 2) != esphome::i2c::ERROR_OK) {
		return -1;
	}

	/* Give Trill time to process this command */
	esphome::delay(25);

	last_read_loc_ = kOffsetCommand;

	uint8_t response[3];
	if (i2c_device_->read(response, 3) != esphome::i2c::ERROR_OK) {
		/* Unexpected or no response; no valid device connected */
		device_type_ = TRILL_NONE;
		firmware_version_ = 0;
		return -1;
	}

	// response[0] is discarded (equivalent to wire_->read())
	device_type_ = (Device)response[1];
	firmware_version_ = response[2];

	return 0;
}

/* Get the name of a given device */
const char* Trill::getNameFromDevice(Device device) {
	switch (device) {
	case TRILL_BAR:
		return "Bar";
	case TRILL_SQUARE:
		return "Square";
	case TRILL_RING:
		return "Ring";
	case TRILL_FLEX:
		return "Flex";
	case TRILL_HEX:
		return "Hex";
	default:
		return "NO DEVICE";
	}
}

/* Read the latest scan value from the sensor. Returns true on success. */
bool Trill::read() {
	if (CENTROID != mode_)
		return false;
	uint8_t loc = 0;
	uint8_t length = kCentroidLengthDefault;

	/* Set the read location to the right place if needed */
	prepareForDataRead();

	if (device_type_ == TRILL_SQUARE || device_type_ == TRILL_HEX)
		length = kCentroidLength2D;

	if (device_type_ == TRILL_RING)
		length = kCentroidLengthRing;

	uint8_t raw_data[kCentroidLength2D];
	if (i2c_device_->read(raw_data, length) != esphome::i2c::ERROR_OK) {
		return false;
	}

	// Convert bytes to 16-bit values (big-endian)
	for (uint8_t i = 0; i < length / 2; i++) {
		buffer_[i] = (raw_data[i * 2] << 8) | raw_data[i * 2 + 1];
	}

	uint8_t maxNumCentroids = MAX_TOUCH_1D_OR_2D;
	bool ret = true;

	processCentroids(maxNumCentroids);
	if (is2D())
		horizontal.processCentroids(maxNumCentroids);

	return ret;
}

/* Update the baseline value on the sensor */
void Trill::updateBaseline() {
	uint8_t cmd[2] = { kOffsetCommand, kCommandBaselineUpdate };
	i2c_device_->write(cmd, 2);

	last_read_loc_ = kOffsetCommand;
}

/* Request raw data; wrappers for I2C */
bool Trill::requestRawData(uint8_t max_length) {
	uint8_t length = 0;

	prepareForDataRead();

	if (max_length == 0xFF) {
		length = RAW_LENGTH;
	}
	if (length > kRawLength)
		length = kRawLength;

	/* Read all raw data at once into our buffer */
	if (i2c_device_->read(rx_buffer_, length) != esphome::i2c::ERROR_OK) {
		rx_buffer_length_ = 0;
		rx_buffer_index_ = 0;
		return false;
	}

	rx_buffer_length_ = length;
	rx_buffer_index_ = 0;
	raw_bytes_left_ = 0;

	return true;
}

int Trill::rawDataAvailable() {
	/* Raw data items are 2 bytes long; return number of them available */
	return ((rx_buffer_length_ - rx_buffer_index_) >> 1);
}

/* Raw data is in 16-bit big-endian format */
int Trill::rawDataRead() {
	if (rx_buffer_index_ + 1 >= rx_buffer_length_)
		return 0;

	int result = (rx_buffer_[rx_buffer_index_] << 8) | rx_buffer_[rx_buffer_index_ + 1];
	rx_buffer_index_ += 2;

	return result;
}

/* Scan configuration settings */
void Trill::setMode(Mode mode) {
	uint8_t cmd[3] = { kOffsetCommand, kCommandMode, (uint8_t)mode };
	i2c_device_->write(cmd, 3);

	mode_ = mode;
	last_read_loc_ = kOffsetCommand;
	num_touches = 0;
}

void Trill::setScanSettings(uint8_t speed, uint8_t num_bits) {
	if (speed > 3)
		speed = 3;
	if (num_bits < 9)
		num_bits = 9;
	if (num_bits > 16)
		num_bits = 16;

	uint8_t cmd[4] = { kOffsetCommand, kCommandScanSettings, speed, num_bits };
	i2c_device_->write(cmd, 4);

	last_read_loc_ = kOffsetCommand;
}

void Trill::setPrescaler(uint8_t prescaler) {
	uint8_t cmd[3] = { kOffsetCommand, kCommandPrescaler, prescaler };
	i2c_device_->write(cmd, 3);

	last_read_loc_ = kOffsetCommand;
}

void Trill::setNoiseThreshold(uint8_t threshold) {
	uint8_t cmd[3] = { kOffsetCommand, kCommandNoiseThreshold, threshold };
	i2c_device_->write(cmd, 3);

	last_read_loc_ = kOffsetCommand;
}

void Trill::setIDACValue(uint8_t value) {
	uint8_t cmd[3] = { kOffsetCommand, kCommandIdac, value };
	i2c_device_->write(cmd, 3);

	last_read_loc_ = kOffsetCommand;
}

void Trill::setMinimumTouchSize(uint16_t size) {
	uint8_t cmd[4] = { kOffsetCommand, kCommandMinimumSize, (uint8_t)(size >> 8), (uint8_t)(size & 0xFF) };
	i2c_device_->write(cmd, 4);

	last_read_loc_ = kOffsetCommand;
}

void Trill::setAutoScanInterval(uint16_t interval) {
	uint8_t cmd[4] = { kOffsetCommand, kCommandAutoScanInterval, (uint8_t)(interval >> 8), (uint8_t)(interval & 0xFF) };
	i2c_device_->write(cmd, 4);

	last_read_loc_ = kOffsetCommand;
}

/* Prepare the device to read data if it is not already prepared */
void Trill::prepareForDataRead() {
	if (last_read_loc_ != kOffsetData) {
		uint8_t cmd[1] = { kOffsetData };
		i2c_device_->write(cmd, 1);

		last_read_loc_ = kOffsetData;
	}
}

int Trill::getButtonValue(uint8_t button_num)
{
	if (mode_ != CENTROID)
		return -1;
	if (button_num > 1)
		return -1;
	if (device_type_ != TRILL_RING)
		return -1;

	return buffer_[2 * MAX_TOUCH_1D_OR_2D + button_num];
}

unsigned int Trill::getNumChannels()
{
	switch (device_type_) {
	case TRILL_BAR: return kNumChannelsBar;
	case TRILL_RING: return kNumChannelsRing;
	default: return kNumChannelsMax;
	}
}

bool Trill::is1D()
{
	if (CENTROID != mode_)
		return false;
	switch (device_type_) {
	case TRILL_BAR:
	case TRILL_RING:
	case TRILL_CRAFT:
	case TRILL_FLEX:
		return true;
	default:
		return false;
	}
}

bool Trill::is2D()
{
	if (CENTROID != mode_)
		return false;
	switch (device_type_) {
	case TRILL_SQUARE:
	case TRILL_HEX:
		return true;
	default:
		return false;
	}
}

uint8_t Touches::getNumTouches() const
{
	return num_touches;
}

int Touches::touchLocation(uint8_t touch_num) const
{
	if (touch_num < num_touches)
		return centroids[touch_num];
	else
		return -1;
}

int Touches::touchSize(uint8_t touch_num) const
{
	if (touch_num < num_touches)
		return sizes[touch_num];
	else
		return -1;
}

unsigned int Touches2D::getNumHorizontalTouches() {
	return horizontal.getNumTouches();
}

void Touches::processCentroids(uint8_t maxCentroids) {
	// Look for 1st instance of 0xFFFF (no touch) in the buffer
	for (num_touches = 0; num_touches < maxCentroids; ++num_touches)
	{
		if (0xffff == centroids[num_touches])
			break;// at the first non-touch, break
	}
	// now num_touches is the number of active touches in the array
}

/* These methods for horizontal touches on 2D sliders */
int Touches2D::touchHorizontalLocation(uint8_t touch_num) {
	return horizontal.touchLocation(touch_num);
}

int Touches2D::touchHorizontalSize(uint8_t touch_num) {
	return horizontal.touchSize(touch_num);
}